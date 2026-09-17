// SPDX-License-Identifier: Apache-2.0
#include "Report.h"

#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace vmdinspect {

namespace {

using namespace motionVmd;

std::string
Hex(std::string_view bytes)
{
    static constexpr char kDigits[] = "0123456789ABCDEF";
    std::string out;
    for (char c : bytes) {
        const auto b = static_cast<unsigned char>(c);
        out += kDigits[b >> 4];
        out += kDigits[b & 0x0F];
    }
    return out;
}

/// A name as a person reads it: its text, or its bytes when it has none.
std::string
NameText(const Name& name)
{
    if (!name.text.empty() || name.bytes.empty()) {
        return name.text;
    }
    return "<bytes " + Hex(name.bytes) + ">";
}

/// The first and last frame of keys sorted by frame.
template <class Keys>
std::string
FrameRange(const Keys& keys)
{
    if (keys.empty()) {
        return "";
    }
    return keys.front().frame == keys.back().frame
               ? "frame " + std::to_string(keys.front().frame)
               : "frames " + std::to_string(keys.front().frame) + "-" +
                     std::to_string(keys.back().frame);
}

template <class Tracks>
std::size_t
KeyCount(const Tracks& tracks)
{
    std::size_t n = 0;
    for (const auto& track : tracks) {
        n += track.keys.size();
    }
    return n;
}

// --- JSON ----------------------------------------------------------------------

std::string
Quoted(std::string_view text)
{
    std::string out = "\"";
    for (char c : text) {
        const auto u = static_cast<unsigned char>(c);
        switch (c) {
        case '"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            if (u < 0x20) {
                char buffer[8];
                std::snprintf(buffer, sizeof buffer, "\\u%04X", u);
                out += buffer;
            } else {
                out += c; // decoded text is valid UTF-8, and JSON is UTF-8
            }
        }
    }
    return out + "\"";
}

/// A minimal writer: objects and arrays, commas and indentation handled here
/// so the report below reads as its structure.
class Json {
public:
    void BeginObject(std::string_view key = {}) { _Open(key, '{'); }
    void EndObject() { _Close('}'); }
    void BeginArray(std::string_view key = {}) { _Open(key, '['); }
    void EndArray() { _Close(']'); }

    void String(std::string_view key, std::string_view value) { _Value(key, Quoted(value)); }
    void Number(std::string_view key, long long value) { _Value(key, std::to_string(value)); }
    void Bool(std::string_view key, bool value) { _Value(key, value ? "true" : "false"); }
    void Null(std::string_view key) { _Value(key, "null"); }

    std::string Take() { return std::move(_out) + "\n"; }

private:
    void _Separate(std::string_view key)
    {
        if (!_first.empty()) {
            _out += _first.back() ? "\n" : ",\n";
            _first.back() = false;
            _out.append(2 * _first.size(), ' ');
        }
        if (!key.empty()) {
            _out += Quoted(key) + ": ";
        }
    }
    void _Open(std::string_view key, char bracket)
    {
        _Separate(key);
        _out += bracket;
        _first.push_back(true);
    }
    void _Close(char bracket)
    {
        const bool empty = _first.back();
        _first.pop_back();
        if (!empty) {
            _out += "\n";
            _out.append(2 * _first.size(), ' ');
        }
        _out += bracket;
    }
    void _Value(std::string_view key, const std::string& value)
    {
        _Separate(key);
        _out += value;
    }

    std::string _out;
    std::vector<bool> _first;
};

void
JsonDiagnostic(Json& json, const Diagnostic& d, std::string_view key = {})
{
    json.BeginObject(key);
    json.String("code", d.code);
    json.String("severity", ToString(d.severity));
    json.String("message", d.message);
    json.String("location", ToString(d.location));
    json.EndObject();
}

void
JsonName(Json& json, std::string_view key, const Name& name)
{
    json.BeginObject(key);
    json.String("text", name.text);
    json.String("bytes", Hex(name.bytes));
    json.EndObject();
}

template <class Keys>
void
JsonRange(Json& json, const Keys& keys)
{
    json.Number("keys", static_cast<long long>(keys.size()));
    if (keys.empty()) {
        json.Null("firstFrame");
        json.Null("lastFrame");
    } else {
        json.Number("firstFrame", keys.front().frame);
        json.Number("lastFrame", keys.back().frame);
    }
}

template <class Tracks, class Member>
void
JsonTracks(Json& json, std::string_view key, const Tracks& tracks, Member member)
{
    json.BeginArray(key);
    for (const auto& track : tracks) {
        json.BeginObject();
        JsonName(json, "name", track.*member);
        JsonRange(json, track.keys);
        json.EndObject();
    }
    json.EndArray();
}

template <class Keys>
void
JsonScene(Json& json, std::string_view key, const Keys& keys)
{
    json.BeginObject(key);
    JsonRange(json, keys);
    json.EndObject();
}

// --- text ----------------------------------------------------------------------

template <class Tracks, class Member>
void
TextTracks(std::string& out, std::string_view title, const Tracks& tracks, Member member,
           std::size_t records, const ReportOptions& options)
{
    out += std::string(title) + ": " + std::to_string(records) +
           (records == 1 ? " keyframe" : " keyframes") + " in " + std::to_string(tracks.size()) +
           (tracks.size() == 1 ? " track\n" : " tracks\n");
    if (!options.tracks) {
        return;
    }
    for (const auto& track : tracks) {
        out += "  " + NameText(track.*member) + "  " + std::to_string(track.keys.size()) +
               (track.keys.size() == 1 ? " key, " : " keys, ") + FrameRange(track.keys) + "\n";
    }
}

template <class Keys>
void
TextScene(std::string& out, std::string_view title, const Keys& keys, std::size_t records)
{
    out += std::string(title) + ": " + std::to_string(records) +
           (records == 1 ? " keyframe" : " keyframes");
    if (!keys.empty()) {
        out += " (" + FrameRange(keys) + ")";
    }
    out += "\n";
}

} // namespace

Inspection
Inspect(Result<Document> read)
{
    Inspection out{std::move(read), std::nullopt, {}};
    out.diagnostics = out.read.diagnostics();
    if (out.read.ok()) {
        auto built = BuildMotion(out.read.value());
        out.diagnostics.insert(
            out.diagnostics.end(), built.diagnostics().begin(), built.diagnostics().end());
        out.motion = std::move(built).value();
    }
    return out;
}

std::string
TextReport(const std::string& file, const Inspection& inspection, const ReportOptions& options)
{
    std::string out = "file:      " + file + "\n";
    if (!inspection.read.ok()) {
        out += "status:    not read\n";
    } else {
        const Document& doc = inspection.read.value();
        const Motion& motion = *inspection.motion;
        out += "signature: " + std::string(ToString(doc.header.version)) + "\n";
        out += "model:     " + NameText(doc.header.modelName) + "\n";
        out += "sections:  " + std::to_string(doc.sectionsPresent) + " of " +
               std::to_string(kSectionCount) + "\n";
        TextTracks(out,
                   "bone keyframes",
                   motion.bones,
                   &BoneTrack::bone,
                   doc.boneKeyframes.size(),
                   options);
        TextTracks(out,
                   "morph keyframes",
                   motion.morphs,
                   &MorphTrack::morph,
                   doc.morphKeyframes.size(),
                   options);
        TextScene(out, "camera keyframes", motion.camera, doc.cameraKeyframes.size());
        TextScene(out, "light keyframes", motion.light, doc.lightKeyframes.size());
        TextScene(out, "self-shadow keyframes", motion.selfShadow, doc.selfShadowKeyframes.size());
        TextScene(out, "visibility keyframes", motion.visibility, doc.ikKeyframes.size());
        TextTracks(out, "IK states", motion.ik, &IkTrack::bone, KeyCount(motion.ik), options);
    }
    if (inspection.diagnostics.empty() && inspection.read.ok()) {
        out += "diagnostics: none\n";
    } else {
        out += "diagnostics:\n";
        for (const Diagnostic& d : inspection.diagnostics) {
            out += "  " + std::string(ToString(d.severity)) + "  " + FormatDiagnostic(d) + "\n";
        }
        if (const Diagnostic* fatal = inspection.read.fatal()) {
            out += "  fatal  " + FormatDiagnostic(*fatal) + "\n";
        }
    }
    return out;
}

std::string
JsonReport(const Inspection& inspection)
{
    Json json;
    json.BeginObject();
    json.Bool("ok", inspection.read.ok());
    if (inspection.read.ok()) {
        const Document& doc = inspection.read.value();
        const Motion& motion = *inspection.motion;

        json.BeginObject("header");
        json.String("signature", ToString(doc.header.version));
        JsonName(json, "modelName", doc.header.modelName);
        json.Number("sectionsPresent", static_cast<long long>(doc.sectionsPresent));
        json.EndObject();

        json.BeginObject("counts");
        json.Number("boneKeyframes", static_cast<long long>(doc.boneKeyframes.size()));
        json.Number("morphKeyframes", static_cast<long long>(doc.morphKeyframes.size()));
        json.Number("cameraKeyframes", static_cast<long long>(doc.cameraKeyframes.size()));
        json.Number("lightKeyframes", static_cast<long long>(doc.lightKeyframes.size()));
        json.Number("selfShadowKeyframes", static_cast<long long>(doc.selfShadowKeyframes.size()));
        json.Number("ikKeyframes", static_cast<long long>(doc.ikKeyframes.size()));
        json.EndObject();

        json.BeginObject("tracks");
        JsonTracks(json, "bones", motion.bones, &BoneTrack::bone);
        JsonTracks(json, "morphs", motion.morphs, &MorphTrack::morph);
        JsonTracks(json, "ik", motion.ik, &IkTrack::bone);
        JsonScene(json, "visibility", motion.visibility);
        JsonScene(json, "camera", motion.camera);
        JsonScene(json, "light", motion.light);
        JsonScene(json, "selfShadow", motion.selfShadow);
        json.EndObject();
    }
    if (const Diagnostic* fatal = inspection.read.fatal()) {
        JsonDiagnostic(json, *fatal, "fatal");
    } else {
        json.Null("fatal");
    }
    json.BeginArray("diagnostics");
    for (const Diagnostic& d : inspection.diagnostics) {
        JsonDiagnostic(json, d);
    }
    json.EndArray();
    json.EndObject();
    return json.Take();
}

int
ExitStatus(const Inspection& inspection)
{
    if (!inspection.read.ok()) {
        return 2;
    }
    for (const Diagnostic& d : inspection.diagnostics) {
        if (d.severity == Severity::Error) {
            return 1;
        }
    }
    return 0;
}

} // namespace vmdinspect
