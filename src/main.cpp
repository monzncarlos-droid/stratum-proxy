// main.cpp - Universal Stratum Proxy
// Based on rinpool-proxy, but uses the coin core's hash function via coin_hash_wrapper.
// This allows the proxy to work with any Bitcoin/Litecoin-forked coin.
//
// Configuration is loaded from a TOML file passed as the single command-line argument.

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <chrono>
#include <cerrno>
#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <openssl/sha.h>

#define TOML_EXCEPTIONS 0
#include <toml++/toml.hpp>

#include <endian.hpp>
#include <hex.h>

// Use the coin core hash wrapper instead of coin-specific hash
#include "coin_hash_wrapper.h"

namespace {

// =============================================================================
// Decorator System (pure regex search & replace)
// =============================================================================

struct Decorator {
  std::string name;       // Human-readable name for documentation
  std::regex pattern;     // Regex pattern to match
  std::string modifier;   // Replacement string (can use $0, $1, $2, etc.)
};

// =============================================================================
// Configuration
// =============================================================================

struct Config {
  std::string upstream_url;
  uint16_t local_port = 0;
  std::string log_file;
  bool color_console = true;
  std::vector<Decorator> decorators;
};

static std::optional<Config> load_config(const std::string& path) {
  auto result = toml::parse_file(path);
  if (!result) {
    std::cerr << "Failed to parse config file: " << path << "\n";
    std::cerr << "Error: " << result.error() << "\n";
    return std::nullopt;
  }

  const toml::table& tbl = result.table();
  Config cfg;

  // Required: upstream_url
  if (auto v = tbl["upstream_url"].value<std::string>()) {
    cfg.upstream_url = *v;
  } else {
    std::cerr << "Config error: 'upstream_url' is required\n";
    return std::nullopt;
  }

  // Required: local_port
  if (auto v = tbl["local_port"].value<int64_t>()) {
    if (*v <= 0 || *v > 65535) {
      std::cerr << "Config error: 'local_port' must be 1-65535\n";
      return std::nullopt;
    }
    cfg.local_port = static_cast<uint16_t>(*v);
  } else {
    std::cerr << "Config error: 'local_port' is required\n";
    return std::nullopt;
  }

  // Optional: log_file
  if (auto v = tbl["log_file"].value<std::string>()) {
    cfg.log_file = *v;
  }

  // Optional: color_console (default true)
  if (auto v = tbl["color_console"].value<bool>()) {
    cfg.color_console = *v;
  }

  // Optional: decorators array
  if (auto decorators_arr = tbl["decorators"].as_array()) {
    for (const auto& elem : *decorators_arr) {
      if (const auto* dec_tbl = elem.as_table()) {
        Decorator dec;

        if (auto v = (*dec_tbl)["name"].value<std::string>()) {
          dec.name = *v;
        }

        auto pattern_opt = (*dec_tbl)["pattern"].value<std::string>();
        auto modifier_opt = (*dec_tbl)["modifier"].value<std::string>();

        if (!pattern_opt) {
          std::cerr << "Config warning: Decorator '" << dec.name << "' missing 'pattern', skipping\n";
          continue;
        }
        if (!modifier_opt) {
          std::cerr << "Config warning: Decorator '" << dec.name << "' missing 'modifier', skipping\n";
          continue;
        }

        try {
          dec.pattern = std::regex(*pattern_opt, std::regex::ECMAScript | std::regex::optimize);
        } catch (const std::regex_error& e) {
          std::cerr << "Config warning: Invalid regex pattern in decorator '"
                    << dec.name << "': " << e.what() << "\n";
          continue;
        }

        dec.modifier = *modifier_opt;

        cfg.decorators.push_back(std::move(dec));
      }
    }
  }

  return cfg;
}

// =============================================================================
// Logger
// =============================================================================

struct Logger {
  std::ostream* console = nullptr;
  std::ofstream* file = nullptr;
  bool color_console = false;
  const std::vector<Decorator>* decorators = nullptr;

  // Apply all decorators in sequence (regex search & replace)
  std::string apply_decorators(const std::string& line) const {
    if (!decorators || decorators->empty()) {
      return line;
    }

    std::string result = line;
    for (const auto& dec : *decorators) {
      result = std::regex_replace(result, dec.pattern, dec.modifier);
    }
    return result;
  }
};

struct Upstream {
  std::string host;
  std::string port;
};

static void usage(const char* argv0) {
  std::cerr
      << "Usage:\n"
      << "  " << argv0 << " <config_file>\n\n"
      << "Example:\n"
      << "  " << argv0 << " stratum.toml\n\n"
      << "See stratum-sample.toml for configuration options.\n";
}

static std::string now_utc_timestamp_ms() {
  using namespace std::chrono;
  const auto now = system_clock::now();
  const auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

  std::time_t tt = system_clock::to_time_t(now);
  std::tm tm_utc{};
  gmtime_r(&tt, &tm_utc);

  std::ostringstream oss;
  oss << std::put_time(&tm_utc, "%Y-%m-%dT%H:%M:%S")
      << '.' << std::setw(3) << std::setfill('0') << ms.count() << 'Z';
  return oss.str();
}

static void console_banner(Logger& logger, std::string_view msg) {
  if (!logger.console) return;
  if (logger.color_console) {
    (*logger.console) << "\x1b[97m" << msg << "\x1b[0m";
  } else {
    (*logger.console) << msg;
  }
  logger.console->flush();
}

static void log_line(Logger& logger, std::string_view origin, std::string_view payload) {
  const std::string ts = now_utc_timestamp_ms();

  // Build the full line (without color)
  std::string line;
  line.reserve(ts.size() + origin.size() + payload.size() + 3);
  line.append(ts);
  line.push_back('\t');
  line.append(origin);
  line.push_back('\t');
  line.append(payload);

  // Write to file (no colors)
  if (logger.file) {
    logger.file->write(line.data(), (std::streamsize)line.size());
    logger.file->write("\n", 1);
    logger.file->flush();
  }

  // Write to console (with decorators if enabled)
  if (logger.console) {
    if (logger.color_console) {
      std::string decorated = logger.apply_decorators(line);
      decorated.push_back('\n');
      logger.console->write(decorated.data(), (std::streamsize)decorated.size());
    } else {
      logger.console->write(line.data(), (std::streamsize)line.size());
      logger.console->write("\n", 1);
    }
    logger.console->flush();
  }
}

// -----------------------------
// Share evaluation structures
// -----------------------------

struct BtcStratumJob {
  bool valid = false;

  std::string job_id;
  std::vector<uint8_t> prev_hash; // 32 bytes, as provided by pool
  std::string coinb1_hex;
  std::string coinb2_hex;
  std::vector<std::string> merkle_branch_hex;

  uint32_t version = 0;
  uint32_t nbits = 0;
  uint32_t ntime = 0;
  bool clean_jobs = false;
};

struct BtcStratumSession {
  bool has_extranonce1 = false;
  std::vector<uint8_t> extranonce1; // binary
  int extranonce2_size = 0;

  bool has_difficulty = false;
  double difficulty = 0.0;

  BtcStratumJob last_job;
};

static std::vector<uint8_t> sha256d(const std::vector<uint8_t>& input) {
  std::vector<uint8_t> hash(32);
  std::vector<uint8_t> temp(32);

  SHA256_CTX ctx;
  SHA256_Init(&ctx);
  SHA256_Update(&ctx, input.data(), input.size());
  SHA256_Final(temp.data(), &ctx);

  SHA256_Init(&ctx);
  SHA256_Update(&ctx, temp.data(), temp.size());
  SHA256_Final(hash.data(), &ctx);

  return hash;
}

static std::vector<uint8_t> calculate_merkle_root(const std::vector<uint8_t>& coinbase,
                                                  const std::vector<std::string>& branches_hex) {
  std::vector<uint8_t> root = sha256d(coinbase);
  for (const auto& bh : branches_hex) {
    std::vector<uint8_t> branch(32);
    if (bh.size() != 64) {
      continue;
    }
    hexstrToBytes(bh, branch.data());

    std::vector<uint8_t> combined;
    combined.reserve(64);
    combined.insert(combined.end(), root.begin(), root.end());
    combined.insert(combined.end(), branch.begin(), branch.end());
    root = sha256d(combined);
  }
  return root;
}

static bool parse_hex_u32_le(const std::string& hex8, uint32_t& out_u32) {
  if (hex8.size() != 8) return false;
  try {
    // Parse hex string as a numeric value (not as LE-encoded bytes)
    out_u32 = (uint32_t)std::stoul(hex8, nullptr, 16);
  } catch (...) {
    return false;
  }
  return true;
}

namespace jsonmini {

enum class Type { Invalid, Null, Bool, Number, String, Array };

struct Value {
  Type type = Type::Invalid;
  std::string s;
  double num = 0.0;
  bool b = false;
  std::vector<Value> a;
};

static inline void skip_ws(const std::string& in, size_t& i) {
  while (i < in.size()) {
    const char c = in[i];
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
      ++i;
      continue;
    }
    break;
  }
}

static bool parse_string(const std::string& in, size_t& i, std::string& out) {
  skip_ws(in, i);
  if (i >= in.size() || in[i] != '"') return false;
  ++i;
  out.clear();
  while (i < in.size()) {
    char c = in[i++];
    if (c == '"') return true;
    if (c == '\\') {
      if (i >= in.size()) return false;
      char e = in[i++];
      switch (e) {
        case '"': out.push_back('"'); break;
        case '\\': out.push_back('\\'); break;
        case '/': out.push_back('/'); break;
        case 'b': out.push_back('\b'); break;
        case 'f': out.push_back('\f'); break;
        case 'n': out.push_back('\n'); break;
        case 'r': out.push_back('\r'); break;
        case 't': out.push_back('\t'); break;
        case 'u':
          if (i + 4 <= in.size()) i += 4;
          out.push_back('?');
          break;
        default:
          out.push_back(e);
          break;
      }
    } else {
      out.push_back(c);
    }
  }
  return false;
}

static bool parse_number(const std::string& in, size_t& i, double& out) {
  skip_ws(in, i);
  size_t start = i;
  if (i < in.size() && (in[i] == '-' || in[i] == '+')) ++i;
  bool any = false;
  while (i < in.size() && (in[i] >= '0' && in[i] <= '9')) {
    ++i;
    any = true;
  }
  if (i < in.size() && in[i] == '.') {
    ++i;
    while (i < in.size() && (in[i] >= '0' && in[i] <= '9')) {
      ++i;
      any = true;
    }
  }
  if (i < in.size() && (in[i] == 'e' || in[i] == 'E')) {
    ++i;
    if (i < in.size() && (in[i] == '-' || in[i] == '+')) ++i;
    while (i < in.size() && (in[i] >= '0' && in[i] <= '9')) {
      ++i;
      any = true;
    }
  }
  if (!any) return false;
  try {
    out = std::stod(in.substr(start, i - start));
  } catch (...) {
    out = 0.0;
  }
  return true;
}

static bool parse_literal(const std::string& in, size_t& i, const char* lit) {
  skip_ws(in, i);
  const size_t n = strlen(lit);
  if (i + n > in.size()) return false;
  if (in.compare(i, n, lit) != 0) return false;
  i += n;
  return true;
}

static bool parse_value(const std::string& in, size_t& i, Value& out);

static bool parse_array(const std::string& in, size_t& i, std::vector<Value>& out) {
  skip_ws(in, i);
  if (i >= in.size() || in[i] != '[') return false;
  ++i;
  out.clear();
  skip_ws(in, i);
  if (i < in.size() && in[i] == ']') {
    ++i;
    return true;
  }
  for (;;) {
    Value v;
    if (!parse_value(in, i, v)) return false;
    out.emplace_back(std::move(v));
    skip_ws(in, i);
    if (i >= in.size()) return false;
    if (in[i] == ',') {
      ++i;
      continue;
    }
    if (in[i] == ']') {
      ++i;
      return true;
    }
    return false;
  }
}

static bool parse_value(const std::string& in, size_t& i, Value& out) {
  skip_ws(in, i);
  if (i >= in.size()) return false;

  if (in[i] == '"') {
    out.type = Type::String;
    return parse_string(in, i, out.s);
  }
  if (in[i] == '[') {
    out.type = Type::Array;
    return parse_array(in, i, out.a);
  }
  if (in[i] == '{') {
    int depth = 0;
    while (i < in.size()) {
      char c = in[i++];
      if (c == '"') {
        std::string tmp;
        --i;
        if (!parse_string(in, i, tmp)) return false;
        continue;
      }
      if (c == '{') depth++;
      if (c == '}') {
        depth--;
        if (depth <= 0) break;
      }
    }
    out.type = Type::Invalid;
    return true;
  }

  if (parse_literal(in, i, "true")) {
    out.type = Type::Bool;
    out.b = true;
    return true;
  }
  if (parse_literal(in, i, "false")) {
    out.type = Type::Bool;
    out.b = false;
    return true;
  }
  if (parse_literal(in, i, "null")) {
    out.type = Type::Null;
    return true;
  }

  double d = 0.0;
  if (parse_number(in, i, d)) {
    out.type = Type::Number;
    out.num = d;
    return true;
  }

  out.type = Type::Invalid;
  return false;
}

static bool find_key_value_start(const std::string& in, const char* key, size_t& out_pos) {
  const std::string needle = std::string("\"") + key + "\"";
  size_t pos = in.find(needle);
  if (pos == std::string::npos) return false;
  pos += needle.size();
  skip_ws(in, pos);
  if (pos >= in.size() || in[pos] != ':') return false;
  ++pos;
  out_pos = pos;
  return true;
}

static bool get_string_field(const std::string& in, const char* key, std::string& out) {
  size_t pos = 0;
  if (!find_key_value_start(in, key, pos)) return false;
  return parse_string(in, pos, out);
}

static bool get_array_field(const std::string& in, const char* key, std::vector<Value>& out) {
  size_t pos = 0;
  if (!find_key_value_start(in, key, pos)) return false;
  return parse_array(in, pos, out);
}

} // namespace jsonmini

static void handle_pool_line_phase2(BtcStratumSession& sess, Logger& log,
                                   const std::string& payload) {
  std::string method;
  (void)jsonmini::get_string_field(payload, "method", method);

  // mining.set_difficulty
  try {
    if (method == "mining.set_difficulty") {
      std::vector<jsonmini::Value> params;
      if (jsonmini::get_array_field(payload, "params", params) && !params.empty() &&
          params[0].type == jsonmini::Type::Number) {
        sess.difficulty = params[0].num;
        sess.has_difficulty = true;
        std::ostringstream oss;
        oss << "{\"type\":\"pool_difficulty\",\"difficulty\":" << std::setprecision(12) << sess.difficulty << "}";
        log_line(log, "proxy", oss.str());
      }
      return;
    }
  } catch (...) {
  }

  // Response to mining.subscribe: capture extranonce1 and extranonce2_size.
  try {
    std::vector<jsonmini::Value> result;
    if (jsonmini::get_array_field(payload, "result", result) && result.size() >= 3 &&
        result[1].type == jsonmini::Type::String && result[2].type == jsonmini::Type::Number) {
      std::string en1 = result[1].s;
      int en2sz = (int)result[2].num;
      if (!en1.empty() && en2sz > 0) {
        sess.extranonce1.resize(en1.size() / 2);
        hexstrToBytes(en1, sess.extranonce1.data());
        sess.extranonce2_size = en2sz;
        sess.has_extranonce1 = true;
        std::ostringstream oss;
        oss << "{\"type\":\"subscribe_result\",\"extranonce1_hex\":\"" << en1
            << "\",\"extranonce2_size\":" << sess.extranonce2_size << "}";
        log_line(log, "proxy", oss.str());
      }
    }
  } catch (...) {
  }

  // mining.notify (BTC-family)
  try {
    if (method == "mining.notify") {
      std::vector<jsonmini::Value> params;
      if (!jsonmini::get_array_field(payload, "params", params)) return;
      if (params.size() < 9) return;
      if (params[0].type != jsonmini::Type::String) return;
      if (params[1].type != jsonmini::Type::String) return;
      if (params[2].type != jsonmini::Type::String) return;
      if (params[3].type != jsonmini::Type::String) return;
      if (params[4].type != jsonmini::Type::Array) return;
      if (params[5].type != jsonmini::Type::String) return;
      if (params[6].type != jsonmini::Type::String) return;
      if (params[7].type != jsonmini::Type::String) return;

      BtcStratumJob job;
      job.job_id = params[0].s;

      std::string prev_hex = params[1].s;
      job.prev_hash.resize(32);
      if (prev_hex.size() != 64) return;
      hexstrToBytes(prev_hex, job.prev_hash.data());

      job.coinb1_hex = params[2].s;
      job.coinb2_hex = params[3].s;

      job.merkle_branch_hex.clear();
      for (const auto& e : params[4].a) {
        if (e.type == jsonmini::Type::String) job.merkle_branch_hex.emplace_back(e.s);
      }

      // version, nbits, ntime are hex strings in little-endian byte order
      if (!parse_hex_u32_le(params[5].s, job.version)) return;
      if (!parse_hex_u32_le(params[6].s, job.nbits)) return;
      if (!parse_hex_u32_le(params[7].s, job.ntime)) return;
      job.clean_jobs = (params[8].type == jsonmini::Type::Bool) ? params[8].b : false;
      job.valid = true;

      sess.last_job = std::move(job);

      std::ostringstream oss;
      oss << "{\"type\":\"job_cached\",\"job_id\":\"" << sess.last_job.job_id
          << "\",\"clean_jobs\":" << (sess.last_job.clean_jobs ? "true" : "false") << "}";
      log_line(log, "proxy", oss.str());
      return;
    }
  } catch (...) {
  }
}

static void handle_miner_line_phase2(BtcStratumSession& sess, Logger& log,
                                    const std::string& payload) {
  std::string method;
  if (!jsonmini::get_string_field(payload, "method", method)) return;
  if (method != "mining.submit") return;

  try {
    std::vector<jsonmini::Value> params;
    if (!jsonmini::get_array_field(payload, "params", params)) return;
    if (params.size() < 5) return;
    if (params[0].type != jsonmini::Type::String) return;
    if (params[1].type != jsonmini::Type::String) return;
    if (params[2].type != jsonmini::Type::String) return;
    if (params[3].type != jsonmini::Type::String) return;
    if (params[4].type != jsonmini::Type::String) return;

    const std::string worker = params[0].s;
    const std::string job_id = params[1].s;
    const std::string extranonce2_hex = params[2].s;
    const std::string ntime_hex = params[3].s;
    const std::string nonce_hex = params[4].s;

    if (!sess.last_job.valid) {
      log_line(log, "proxy", "{\"type\":\"share_eval\",\"error\":\"no_cached_job\"}");
      return;
    }
    if (!sess.has_extranonce1) {
      log_line(log, "proxy", "{\"type\":\"share_eval\",\"error\":\"missing_extranonce1\"}");
      return;
    }
    if (job_id != sess.last_job.job_id) {
      std::ostringstream oss;
      oss << "{\"type\":\"share_eval\",\"error\":\"job_id_mismatch\",\"submit_job_id\":\"" << job_id
          << "\",\"cached_job_id\":\"" << sess.last_job.job_id << "\"}";
      log_line(log, "proxy", oss.str());
    }

    // Assemble coinbase = coinb1 + extranonce1 + extranonce2 + coinb2
    const std::string coinbase_hex = sess.last_job.coinb1_hex + hexStr(sess.extranonce1.data(), sess.extranonce1.size()) + extranonce2_hex + sess.last_job.coinb2_hex;
    std::vector<uint8_t> coinbase(coinbase_hex.size() / 2);
    hexstrToBytes(coinbase_hex, coinbase.data());

    std::vector<uint8_t> merkle_root = calculate_merkle_root(coinbase, sess.last_job.merkle_branch_hex);

    // Build block header (80 bytes)
    // Layout: version(4) + prevhash(32) + merkleroot(32) + ntime(4) + nbits(4) + nonce(4)
    std::array<uint8_t, 80> hdr{};
    
    // Version (little-endian)
    le32enc(hdr.data() + 0, sess.last_job.version);

    // Previous block hash - pool sends hex in chunks already, but each 4-byte
    // word needs to be byte-swapped for CBlockHeader's uint256 format.
    // This matches what tnn-miner does with swap_prev_hash=true for scrypt-like coins.
    for (int i = 0; i < 8; i++) {
      // Read 4 bytes from prevhash
      uint32_t word;
      std::memcpy(&word, sess.last_job.prev_hash.data() + i * 4, 4);
      // Swap bytes within the word
      word = ((word >> 24) & 0xFF) | ((word >> 8) & 0xFF00) |
             ((word << 8) & 0xFF0000) | ((word << 24) & 0xFF000000);
      // Write as little-endian
      le32enc(hdr.data() + 4 + i * 4, word);
    }

    // Merkle root - already in correct byte order from SHA256d
    std::memcpy(hdr.data() + 36, merkle_root.data(), 32);

    // nTime
    uint32_t ntime_val = sess.last_job.ntime;
    if (ntime_hex.size() == 8) {
      ntime_val = (uint32_t)std::stoul(ntime_hex, nullptr, 16);
    }
    le32enc(hdr.data() + 68, ntime_val);

    // nBits
    le32enc(hdr.data() + 72, sess.last_job.nbits);

    // Nonce
    uint32_t nonce_val = 0;
    if (nonce_hex.size() == 8) {
      nonce_val = (uint32_t)std::stoul(nonce_hex, nullptr, 16);
    }
    le32enc(hdr.data() + 76, nonce_val);

    // Compute hash using the coin core's hash function
    uint8_t hash32[32];
    CoinHash::computeBlockHash(hdr.data(), hash32);

    const double share_diff = CoinHash::shareDifficultyFromHash(hash32);

    bool meets_pool_diff = false;
    if (sess.has_difficulty) {
      uint32_t target_words[8];
      CoinHash::difficultyToTarget(sess.difficulty, target_words);
      meets_pool_diff = CoinHash::checkHashMeetsTarget(hash32, target_words);
    }

    std::ostringstream oss;
    oss << "{\"type\":\"share_eval\""
        << ",\"worker\":\"" << worker << "\""
        << ",\"job_id\":\"" << job_id << "\""
        << ",\"extranonce2\":\"" << extranonce2_hex << "\""
        << ",\"ntime\":\"" << ntime_hex << "\""
        << ",\"nonce\":\"" << nonce_hex << "\""
        << ",\"hash_hex\":\"" << hexStr(hash32, 32) << "\""
        << ",\"difficulty\":" << std::setprecision(12) << share_diff;

    if (sess.has_difficulty) {
      oss << ",\"pool_difficulty\":" << std::setprecision(12) << sess.difficulty
          << ",\"meets_pool_difficulty\":" << (meets_pool_diff ? "true" : "false");
    }
    oss << "}";
    log_line(log, "proxy", oss.str());
  } catch (...) {
    // Don't let parsing/compute errors break the proxy.
  }
}

static std::string trim(std::string s) {
  auto is_space = [](unsigned char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
  while (!s.empty() && is_space((unsigned char)s.front())) s.erase(s.begin());
  while (!s.empty() && is_space((unsigned char)s.back())) s.pop_back();
  return s;
}

static std::optional<Upstream> parse_upstream(std::string input) {
  input = trim(std::move(input));
  if (input.empty()) return std::nullopt;

  const std::string_view prefixes[] = {"stratum+tcp://", "tcp://"};
  for (auto p : prefixes) {
    if (input.rfind(std::string(p), 0) == 0) {
      input.erase(0, p.size());
      break;
    }
  }

  std::string host;
  std::string port;
  if (!input.empty() && input.front() == '[') {
    auto close = input.find(']');
    if (close == std::string::npos) return std::nullopt;
    host = input.substr(1, close - 1);
    if (close + 1 >= input.size() || input[close + 1] != ':') return std::nullopt;
    port = input.substr(close + 2);
  } else {
    auto pos = input.rfind(':');
    if (pos == std::string::npos) return std::nullopt;
    host = input.substr(0, pos);
    port = input.substr(pos + 1);
  }

  host = trim(std::move(host));
  port = trim(std::move(port));
  if (host.empty() || port.empty()) return std::nullopt;
  return Upstream{host, port};
}

static int set_reuseaddr(int fd) {
  int opt = 1;
  return setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
}

static int listen_on_port(uint16_t port) {
  int fd = ::socket(AF_INET6, SOCK_STREAM, 0);
  if (fd < 0) return -1;

  (void)set_reuseaddr(fd);

  int v6only = 0;
  (void)setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &v6only, sizeof(v6only));

  sockaddr_in6 addr{};
  addr.sin6_family = AF_INET6;
  addr.sin6_addr = in6addr_any;
  addr.sin6_port = htons(port);

  if (::bind(fd, (sockaddr*)&addr, sizeof(addr)) != 0) {
    ::close(fd);
    return -1;
  }

  if (::listen(fd, 16) != 0) {
    ::close(fd);
    return -1;
  }

  return fd;
}

static int connect_to_host(const std::string& host, const std::string& port) {
  addrinfo hints{};
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_family = AF_UNSPEC;

  addrinfo* res = nullptr;
  if (::getaddrinfo(host.c_str(), port.c_str(), &hints, &res) != 0) {
    return -1;
  }

  int fd = -1;
  for (addrinfo* it = res; it != nullptr; it = it->ai_next) {
    fd = ::socket(it->ai_family, it->ai_socktype, it->ai_protocol);
    if (fd < 0) continue;

    if (::connect(fd, it->ai_addr, it->ai_addrlen) == 0) {
      break;
    }

    ::close(fd);
    fd = -1;
  }

  ::freeaddrinfo(res);
  return fd;
}

struct StreamState {
  std::string line_buf;
  std::vector<uint8_t> pending_send;
  bool eof = false;
};

static void append_log_and_handle_lines(StreamState& st,
                                        Logger& log,
                                        BtcStratumSession* phase2,
                                        std::string_view origin,
                                        const uint8_t* data,
                                        size_t len) {
  for (size_t i = 0; i < len; ++i) {
    const char c = (char)data[i];
    if (c == '\n') {
      log_line(log, origin, st.line_buf);

      if (phase2) {
        const std::string line = st.line_buf;
        if (origin == "pool") {
          handle_pool_line_phase2(*phase2, log, line);
        } else if (origin == "miner") {
          handle_miner_line_phase2(*phase2, log, line);
        }
      }

      st.line_buf.clear();
    } else {
      st.line_buf.push_back(c);
    }
  }
}

static void flush_partial_line(StreamState& st, Logger& log, std::string_view origin) {
  if (!st.line_buf.empty()) {
    log_line(log, origin, st.line_buf);
    st.line_buf.clear();
  }
}

static bool recv_into(int fd,
                      StreamState& in,
                      StreamState& out,
                      Logger& log,
                      BtcStratumSession* phase2,
                      std::string_view origin_for_log) {
  uint8_t buf[64 * 1024];
  ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
  if (n == 0) {
    in.eof = true;
    return false;
  }
  if (n < 0) {
    if (errno == EINTR) return true;
    if (errno == EAGAIN || errno == EWOULDBLOCK) return true;
    in.eof = true;
    return false;
  }

  append_log_and_handle_lines(in, log, phase2, origin_for_log, buf, (size_t)n);

  out.pending_send.insert(out.pending_send.end(), buf, buf + n);
  return true;
}

static bool send_pending(int fd, StreamState& st) {
  if (st.pending_send.empty()) return true;
  ssize_t n = ::send(fd, st.pending_send.data(), st.pending_send.size(), MSG_NOSIGNAL);
  if (n < 0) {
    if (errno == EINTR) return true;
    if (errno == EAGAIN || errno == EWOULDBLOCK) return true;
    return false;
  }
  if (n == 0) return true;
  st.pending_send.erase(st.pending_send.begin(), st.pending_send.begin() + n);
  return true;
}

} // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    usage(argv[0]);
    return 2;
  }

  const std::string config_path = argv[1];

  // Load configuration from TOML file
  auto config_opt = load_config(config_path);
  if (!config_opt) {
    return 2;
  }
  Config& config = *config_opt;

  // Parse upstream URL
  auto upstream = parse_upstream(config.upstream_url);
  if (!upstream) {
    std::cerr << "Invalid upstream URL: " << config.upstream_url << "\n";
    return 2;
  }

  // Open log file if specified
  std::optional<std::ofstream> file_log;
  if (!config.log_file.empty()) {
    file_log.emplace(config.log_file, std::ios::binary | std::ios::app);
    if (!file_log->is_open()) {
      std::cerr << "Failed to open log file: " << config.log_file << "\n";
      return 2;
    }
  }

  // Setup logger
  Logger logger;
  logger.console = &std::cout;
  logger.file = file_log ? &(*file_log) : nullptr;
  logger.color_console = config.color_console && (::isatty(STDOUT_FILENO) == 1);
  logger.decorators = &config.decorators;

  // Start listening
  int lfd = listen_on_port(config.local_port);
  if (lfd < 0) {
    std::cerr << "Failed to listen on port " << config.local_port << " (errno=" << errno << ")\n";
    return 1;
  }

  {
    std::ostringstream oss;
    oss << "Stratum-Proxy listening on port " << config.local_port << "\n";
    oss << "Config: " << config_path << "\n";
    oss << "Upstream: " << upstream->host << ":" << upstream->port << "\n";
    oss << "Decorators: " << config.decorators.size() << " loaded\n";
    console_banner(logger, oss.str());
  }
  log_line(logger, "proxy", "listening");

  int cfd = -1;
  int pfd = -1;

  StreamState miner_in, miner_out;
  StreamState pool_in, pool_out;

  BtcStratumSession phase2_sess;

  auto close_client_and_pool = [&]() {
    if (cfd >= 0) ::close(cfd);
    if (pfd >= 0) ::close(pfd);
    cfd = -1;
    pfd = -1;
    miner_in = StreamState{};
    miner_out = StreamState{};
    pool_in = StreamState{};
    pool_out = StreamState{};
  };

  for (;;) {
    std::vector<pollfd> fds;
    fds.reserve(3);

    pollfd p{};
    p.fd = lfd;
    p.events = POLLIN;
    fds.push_back(p);

    if (cfd >= 0) {
      pollfd a{};
      a.fd = cfd;
      a.events = POLLIN;
      if (!miner_out.pending_send.empty()) a.events |= POLLOUT;
      fds.push_back(a);
    }

    if (pfd >= 0) {
      pollfd b{};
      b.fd = pfd;
      b.events = POLLIN;
      if (!pool_out.pending_send.empty()) b.events |= POLLOUT;
      fds.push_back(b);
    }

    int rc = ::poll(fds.data(), (nfds_t)fds.size(), 250);
    if (rc < 0) {
      if (errno == EINTR) continue;
      std::cerr << "poll failed (errno=" << errno << ")\n";
      break;
    }

    if (!fds.empty() && (fds[0].revents & POLLIN)) {
      sockaddr_storage ss{};
      socklen_t slen = sizeof(ss);
      int nfd = ::accept(lfd, (sockaddr*)&ss, &slen);
      if (nfd >= 0) {
        if (cfd >= 0) {
          ::close(nfd);
        } else {
          cfd = nfd;
          pfd = connect_to_host(upstream->host, upstream->port);
          if (pfd < 0) {
            log_line(logger, "proxy", "upstream_connect_failed");
            ::close(cfd);
            cfd = -1;
          } else {
            log_line(logger, "proxy", "session_start");
          }
        }
      }
    }

    if (cfd < 0 || pfd < 0) {
      continue;
    }

    int idx_client = 1;
    int idx_pool = 2;

    bool ok = true;

    if ((fds[idx_client].revents & POLLIN) != 0) {
      ok = recv_into(cfd, miner_in, pool_out, logger, &phase2_sess, "miner");
      if (!ok) {
        flush_partial_line(miner_in, logger, "miner");
      }
    }

    if (ok && (fds[idx_pool].revents & POLLIN) != 0) {
      ok = recv_into(pfd, pool_in, miner_out, logger, &phase2_sess, "pool");
      if (!ok) {
        flush_partial_line(pool_in, logger, "pool");
      }
    }

    if (ok && (fds[idx_pool].revents & POLLOUT) != 0) {
      ok = send_pending(pfd, pool_out);
    }

    if (ok && (fds[idx_client].revents & POLLOUT) != 0) {
      ok = send_pending(cfd, miner_out);
    }

    if (!ok || (fds[idx_client].revents & (POLLERR | POLLHUP | POLLNVAL)) ||
        (fds[idx_pool].revents & (POLLERR | POLLHUP | POLLNVAL))) {
      flush_partial_line(miner_in, logger, "miner");
      flush_partial_line(pool_in, logger, "pool");
      log_line(logger, "proxy", "session_end");
      close_client_and_pool();
    }
  }

  if (cfd >= 0) ::close(cfd);
  if (pfd >= 0) ::close(pfd);
  ::close(lfd);
  return 0;
}
