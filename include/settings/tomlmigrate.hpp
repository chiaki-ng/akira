#pragma once

#include <toml++/toml.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tomlmigrate {

using Version = std::int64_t;

class MigrationError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

struct Options {
    std::string version_key = "version";
    std::optional<Version> assume_missing_version = std::nullopt;
    bool backup = true;
    std::string backup_suffix = ".bak";
};

struct StepResult {
    Version from = 0;
    Version to = 0;
    std::string description;
};

struct Result {
    Version from_version = 0;
    Version to_version = 0;
    std::vector<StepResult> steps;

    bool changed() const { return !steps.empty(); }
};

using StepFn = std::function<void(toml::table&)>;

class Migrator {
public:
    explicit Migrator(Options opts = {}) : opts_(std::move(opts)) {}

    Migrator& step(Version from, std::string description, StepFn fn) {
        if (from < 0)
            throw MigrationError("migration 'from' version must be >= 0");
        auto [it, inserted] = steps_.try_emplace(
            from, Step{from, std::move(description), std::move(fn)});
        (void)it;
        if (!inserted)
            throw MigrationError("duplicate migration step from version " +
                                 std::to_string(from));
        return *this;
    }

    Version latest_version() const {
        if (steps_.empty()) return 0;
        return steps_.rbegin()->first + 1;
    }

    Version read_version(const toml::table& doc) const {
        const auto* node = doc.get(opts_.version_key);
        if (!node) {
            if (opts_.assume_missing_version)
                return *opts_.assume_missing_version;
            throw MigrationError("missing '" + opts_.version_key + "' field");
        }
        auto v = node->value<std::int64_t>();
        if (!v)
            throw MigrationError("'" + opts_.version_key +
                                 "' must be an integer");
        return *v;
    }

    Result migrate(toml::table& doc,
                   std::optional<Version> target = std::nullopt) const {
        const Version tgt = target.value_or(latest_version());
        return run(doc, read_version(doc), tgt);
    }

    std::pair<std::string, Result> migrate_string(
        std::string_view toml_text,
        std::optional<Version> target = std::nullopt) const {
        toml::table doc = toml::parse(toml_text);
        Result res = migrate(doc, target);
        std::ostringstream out;
        out << doc;
        return {out.str(), res};
    }

    Result migrate_file(const std::filesystem::path& path,
                        std::optional<Version> target = std::nullopt,
                        bool dry_run = false) const {
        toml::table doc;
        try {
            doc = toml::parse_file(path.string());
        } catch (const toml::parse_error& e) {
            std::ostringstream msg;
            msg << "failed to parse " << path << ": " << e.description();
            throw MigrationError(msg.str());
        }

        Result res = migrate(doc, target);
        if (dry_run || !res.changed()) return res;

        if (opts_.backup) {
            std::filesystem::path bak = path;
            bak += opts_.backup_suffix;
            std::filesystem::copy_file(
                path, bak, std::filesystem::copy_options::overwrite_existing);
        }

        std::filesystem::path tmp = path;
        tmp += ".tomlmigrate.tmp";
        {
            std::ofstream os(tmp, std::ios::binary | std::ios::trunc);
            if (!os)
                throw MigrationError("cannot open temp file " + tmp.string());
            os << doc << '\n';
            os.flush();
            if (!os)
                throw MigrationError("failed writing " + tmp.string());
        }
        std::filesystem::rename(tmp, path);
        return res;
    }

private:
    struct Step {
        Version from = 0;
        std::string description;
        StepFn fn;
    };

    Result run(toml::table& doc, Version cur, Version tgt) const {
        if (cur == tgt) return Result{cur, cur, {}};
        if (cur > tgt)
            throw MigrationError("cannot downgrade: document is version " +
                                 std::to_string(cur) + ", target is " +
                                 std::to_string(tgt));

        for (Version v = cur; v < tgt; ++v) {
            if (steps_.find(v) == steps_.end())
                throw MigrationError("no migration step from version " +
                                     std::to_string(v) + " (needed to reach " +
                                     std::to_string(tgt) + ")");
        }

        Result res{cur, tgt, {}};
        for (Version v = cur; v < tgt; ++v) {
            const Step& s = steps_.at(v);
            try {
                s.fn(doc);
            } catch (const std::exception& e) {
                throw MigrationError("migration step " + std::to_string(v) +
                                     "->" + std::to_string(v + 1) +
                                     " failed: " + e.what());
            }
            doc.insert_or_assign(opts_.version_key, v + 1);
            res.steps.push_back({v, v + 1, s.description});
        }
        return res;
    }

    Options opts_;
    std::map<Version, Step> steps_;
};

}
