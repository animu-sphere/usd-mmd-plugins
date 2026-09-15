// SPDX-License-Identifier: Apache-2.0
#include "Report.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mmdinspect {

namespace {

using namespace mmd::pmx;

constexpr std::array<std::string_view, 5> kPanels{"hidden", "eyebrow", "eye", "mouth", "other"};
constexpr std::array<std::string_view, 3> kShapes{"sphere", "box", "capsule"};
constexpr std::array<std::string_view, 3> kPhysicsModes{
    "follows bone", "simulated", "simulated, bone aligned"};
constexpr std::array<std::string_view, 6> kJointTypes{
    "spring 6-DOF", "6-DOF", "point-to-point", "cone-twist", "slider", "hinge"};
constexpr std::array<std::string_view, 4> kSphereModes{
    "disabled", "multiply", "add", "sub-texture"};

/// A table's name for a stored byte, or the number when PMX names none.
template <std::size_t N>
std::string
Named(const std::array<std::string_view, N>& names, unsigned value)
{
    return value < N ? std::string(names[value]) : std::to_string(value);
}

std::string
Hex(unsigned value, int digits)
{
    char buffer[16];
    std::snprintf(buffer, sizeof buffer, "0x%0*X", digits, value);
    return buffer;
}

std::string
IndexText(std::int32_t index)
{
    return index == kNoIndex ? "-" : std::to_string(index);
}

std::string_view
EncodingText(TextEncoding encoding)
{
    return encoding == TextEncoding::Utf8 ? "UTF-8" : "UTF-16LE";
}

/// "サンプル (Sample)", or just the name when there is no English one.
std::string
Names(const std::string& name, const std::string& english)
{
    return english.empty() ? name : name + " (" + english + ")";
}

std::map<std::string, std::size_t>
DeformHistogram(const Document& doc)
{
    std::map<std::string, std::size_t> out;
    for (const Vertex& v : doc.vertices) {
        ++out[std::string(ToString(v.deform.type))];
    }
    return out;
}

std::map<std::string, std::size_t>
MorphHistogram(const Document& doc)
{
    std::map<std::string, std::size_t> out;
    for (const Morph& m : doc.morphs) {
        ++out[std::string(ToString(m.type))];
    }
    return out;
}

std::string
Histogram(const std::map<std::string, std::size_t>& counts)
{
    std::string out;
    for (const auto& [name, n] : counts) {
        out += (out.empty() ? "" : ", ") + name + " " + std::to_string(n);
    }
    return out.empty() ? out : "  (" + out + ")";
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
    /// kNoIndex is null: "no element" is not an index.
    void Index(std::string_view key, std::int32_t value)
    {
        if (value == kNoIndex) {
            Null(key);
        } else {
            Number(key, value);
        }
    }

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
JsonDiagnostic(Json& json, const mmd::Diagnostic& d, std::string_view key = {})
{
    json.BeginObject(key);
    json.String("code", d.code);
    json.String("severity", mmd::ToString(d.severity));
    json.String("message", d.message);
    json.String("location", mmd::ToString(d.location));
    json.EndObject();
}

} // namespace

std::string
TextReport(const std::string& file, const mmd::Result<Document>& result,
           const ReportOptions& options)
{
    std::string out = "file:        " + file + "\n";
    if (!result.ok()) {
        out += "status:      not read\n";
    } else {
        const Document& doc = result.value();
        const Globals& g = doc.header.globals;
        out += "format:      PMX " + std::string(ToString(doc.header.version)) + ", " +
               std::string(EncodingText(g.textEncoding)) + ", " +
               std::to_string(g.additionalVec4Count) + " additional vec4\n";
        out += "index bytes: vertex " + std::to_string(g.vertexIndexSize) + ", texture " +
               std::to_string(g.textureIndexSize) + ", material " +
               std::to_string(g.materialIndexSize) + ", bone " + std::to_string(g.boneIndexSize) +
               ", morph " + std::to_string(g.morphIndexSize) + ", rigid body " +
               std::to_string(g.rigidBodyIndexSize) + "\n";
        if (!g.unknown.empty()) {
            out +=
                "globals:     " + std::to_string(g.unknown.size()) + " beyond the 8 PMX defines\n";
        }
        out += "model:       " + Names(doc.model.name, doc.model.englishName) + "\n";

        std::size_t ik = 0;
        for (const Bone& b : doc.bones) {
            ik += (b.flags & BoneFlag::Ik) ? 1 : 0;
        }
        const auto row = [&](std::string_view name, std::size_t n, const std::string& detail) {
            char buffer[64];
            std::snprintf(buffer, sizeof buffer, "  %-15s %8zu", std::string(name).c_str(), n);
            out += buffer + detail + "\n";
        };
        out += "tables:\n";
        row("vertices", doc.vertices.size(), Histogram(DeformHistogram(doc)));
        row("faces", doc.faces.size() / 3, "  (" + std::to_string(doc.faces.size()) + " indices)");
        row("textures", doc.textures.size(), "");
        row("materials", doc.materials.size(), "");
        row("bones", doc.bones.size(), ik ? "  (IK " + std::to_string(ik) + ")" : "");
        row("morphs", doc.morphs.size(), Histogram(MorphHistogram(doc)));
        row("display frames", doc.displayFrames.size(), "");
        row("rigid bodies", doc.rigidBodies.size(), "");
        row("joints", doc.joints.size(), "");
        if (doc.header.version == Version::V2_1) {
            row("soft bodies", doc.softBodies.size(), "");
        }

        if (options.elements) {
            const auto list = [&](std::string_view title, std::size_t n, auto&& line) {
                if (n == 0) {
                    return;
                }
                out += std::string(title) + ":\n";
                for (std::size_t i = 0; i < n; ++i) {
                    out += "  [" + std::to_string(i) + "] " + line(i) + "\n";
                }
            };
            list("textures", doc.textures.size(), [&](std::size_t i) { return doc.textures[i]; });
            list("materials", doc.materials.size(), [&](std::size_t i) {
                const Material& m = doc.materials[i];
                std::string toon = m.toonReference == ToonReference::Shared
                                       ? "shared toon " + std::to_string(m.sharedToon)
                                       : "toon texture " + IndexText(m.toonTexture);
                return Names(m.name, m.englishName) + "  faces " + std::to_string(m.faceCount / 3) +
                       "  texture " + IndexText(m.texture) + "  sphere " +
                       IndexText(m.sphereTexture) + " (" + Named(kSphereModes, m.sphereMode) +
                       ")  " + toon + "  flags " + Hex(m.flags, 2);
            });
            list("bones", doc.bones.size(), [&](std::size_t i) {
                const Bone& b = doc.bones[i];
                std::string line = Names(b.name, b.englishName) + "  parent " +
                                   IndexText(b.parent) + "  layer " +
                                   std::to_string(b.transformLayer) + "  flags " + Hex(b.flags, 4);
                if (b.flags & BoneFlag::Ik) {
                    line += "  IK target " + IndexText(b.ik.target) + ", " +
                            std::to_string(b.ik.links.size()) + " links";
                }
                return line;
            });
            list("morphs", doc.morphs.size(), [&](std::size_t i) {
                const Morph& m = doc.morphs[i];
                return Names(m.name, m.englishName) + "  " + std::string(ToString(m.type)) + ", " +
                       std::to_string(m.OffsetCount()) + " offsets  panel " +
                       Named(kPanels, m.panel);
            });
            list("display frames", doc.displayFrames.size(), [&](std::size_t i) {
                const DisplayFrame& f = doc.displayFrames[i];
                return Names(f.name, f.englishName) + "  " + std::to_string(f.elements.size()) +
                       " elements" + (f.special ? "  special" : "");
            });
            list("rigid bodies", doc.rigidBodies.size(), [&](std::size_t i) {
                const RigidBody& r = doc.rigidBodies[i];
                return Names(r.name, r.englishName) + "  bone " + IndexText(r.bone) + "  " +
                       Named(kShapes, r.shape) + "  " + Named(kPhysicsModes, r.physicsMode);
            });
            list("joints", doc.joints.size(), [&](std::size_t i) {
                const Joint& j = doc.joints[i];
                return Names(j.name, j.englishName) + "  " + Named(kJointTypes, j.type) +
                       "  bodies " + IndexText(j.rigidBodyA) + ", " + IndexText(j.rigidBodyB);
            });
            list("soft bodies", doc.softBodies.size(), [&](std::size_t i) {
                const SoftBody& s = doc.softBodies[i];
                return Names(s.name, s.englishName) + "  material " + IndexText(s.material) + "  " +
                       std::to_string(s.anchors.size()) + " anchors, " +
                       std::to_string(s.pinVertices.size()) + " pins";
            });
        }
    }

    const auto& diagnostics = result.diagnostics();
    if (diagnostics.empty() && result.ok()) {
        out += "diagnostics: none\n";
    } else {
        out += "diagnostics:\n";
        for (const mmd::Diagnostic& d : diagnostics) {
            out += "  " + std::string(mmd::ToString(d.severity)) + "  " + mmd::FormatDiagnostic(d) +
                   "\n";
        }
        if (!result.ok()) {
            out += "  fatal  " + mmd::FormatDiagnostic(*result.fatal()) + "\n";
        }
    }
    return out;
}

std::string
JsonReport(const mmd::Result<Document>& result)
{
    Json json;
    json.BeginObject();
    json.Bool("ok", result.ok());
    if (result.ok()) {
        const Document& doc = result.value();
        const Globals& g = doc.header.globals;

        json.BeginObject("header");
        json.String("version", ToString(doc.header.version));
        json.String("encoding", EncodingText(g.textEncoding));
        json.Number("additionalVec4", g.additionalVec4Count);
        json.BeginObject("indexSizes");
        json.Number("vertex", g.vertexIndexSize);
        json.Number("texture", g.textureIndexSize);
        json.Number("material", g.materialIndexSize);
        json.Number("bone", g.boneIndexSize);
        json.Number("morph", g.morphIndexSize);
        json.Number("rigidBody", g.rigidBodyIndexSize);
        json.EndObject();
        json.BeginArray("unknownGlobals");
        for (std::uint8_t v : g.unknown) {
            json.Number("", v);
        }
        json.EndArray();
        json.EndObject();

        json.BeginObject("model");
        json.String("name", doc.model.name);
        json.String("englishName", doc.model.englishName);
        json.String("comment", doc.model.comment);
        json.String("englishComment", doc.model.englishComment);
        json.EndObject();

        json.BeginObject("counts");
        json.Number("vertices", static_cast<long long>(doc.vertices.size()));
        json.Number("faces", static_cast<long long>(doc.faces.size()));
        json.Number("textures", static_cast<long long>(doc.textures.size()));
        json.Number("materials", static_cast<long long>(doc.materials.size()));
        json.Number("bones", static_cast<long long>(doc.bones.size()));
        json.Number("morphs", static_cast<long long>(doc.morphs.size()));
        json.Number("displayFrames", static_cast<long long>(doc.displayFrames.size()));
        json.Number("rigidBodies", static_cast<long long>(doc.rigidBodies.size()));
        json.Number("joints", static_cast<long long>(doc.joints.size()));
        json.Number("softBodies", static_cast<long long>(doc.softBodies.size()));
        json.EndObject();

        json.BeginObject("deformTypes");
        for (const auto& [name, n] : DeformHistogram(doc)) {
            json.Number(name, static_cast<long long>(n));
        }
        json.EndObject();

        json.BeginArray("textures");
        for (const std::string& path : doc.textures) {
            json.String("", path);
        }
        json.EndArray();

        json.BeginArray("materials");
        for (const Material& m : doc.materials) {
            json.BeginObject();
            json.String("name", m.name);
            json.String("englishName", m.englishName);
            json.Number("flags", m.flags);
            json.Number("faceCount", m.faceCount);
            json.Index("texture", m.texture);
            json.Index("sphereTexture", m.sphereTexture);
            json.Number("sphereMode", m.sphereMode);
            if (m.toonReference == ToonReference::Shared) {
                json.Number("sharedToon", m.sharedToon);
            } else {
                json.Index("toonTexture", m.toonTexture);
            }
            json.EndObject();
        }
        json.EndArray();

        json.BeginArray("bones");
        for (const Bone& b : doc.bones) {
            json.BeginObject();
            json.String("name", b.name);
            json.String("englishName", b.englishName);
            json.Index("parent", b.parent);
            json.Number("transformLayer", b.transformLayer);
            json.Number("flags", b.flags);
            if (b.flags & BoneFlag::Ik) {
                json.Index("ikTarget", b.ik.target);
                json.BeginArray("ikLinks");
                for (const IkLink& link : b.ik.links) {
                    json.Index("", link.bone);
                }
                json.EndArray();
            }
            json.EndObject();
        }
        json.EndArray();

        json.BeginArray("morphs");
        for (const Morph& m : doc.morphs) {
            json.BeginObject();
            json.String("name", m.name);
            json.String("englishName", m.englishName);
            json.String("type", ToString(m.type));
            json.Number("panel", m.panel);
            json.Number("offsets", static_cast<long long>(m.OffsetCount()));
            json.EndObject();
        }
        json.EndArray();

        json.BeginArray("displayFrames");
        for (const DisplayFrame& f : doc.displayFrames) {
            json.BeginObject();
            json.String("name", f.name);
            json.String("englishName", f.englishName);
            json.Number("special", f.special);
            json.BeginArray("elements");
            for (const FrameElement& e : f.elements) {
                json.BeginObject();
                json.String("kind", e.kind == FrameElementKind::Bone ? "bone" : "morph");
                json.Index("index", e.index);
                json.EndObject();
            }
            json.EndArray();
            json.EndObject();
        }
        json.EndArray();

        json.BeginArray("rigidBodies");
        for (const RigidBody& r : doc.rigidBodies) {
            json.BeginObject();
            json.String("name", r.name);
            json.String("englishName", r.englishName);
            json.Index("bone", r.bone);
            json.Number("shape", r.shape);
            json.Number("physicsMode", r.physicsMode);
            json.EndObject();
        }
        json.EndArray();

        json.BeginArray("joints");
        for (const Joint& j : doc.joints) {
            json.BeginObject();
            json.String("name", j.name);
            json.String("englishName", j.englishName);
            json.Number("type", j.type);
            json.Index("rigidBodyA", j.rigidBodyA);
            json.Index("rigidBodyB", j.rigidBodyB);
            json.EndObject();
        }
        json.EndArray();

        json.BeginArray("softBodies");
        for (const SoftBody& s : doc.softBodies) {
            json.BeginObject();
            json.String("name", s.name);
            json.String("englishName", s.englishName);
            json.Index("material", s.material);
            json.Number("anchors", static_cast<long long>(s.anchors.size()));
            json.Number("pinVertices", static_cast<long long>(s.pinVertices.size()));
            json.EndObject();
        }
        json.EndArray();
    }

    json.BeginArray("diagnostics");
    for (const mmd::Diagnostic& d : result.diagnostics()) {
        JsonDiagnostic(json, d);
    }
    json.EndArray();
    if (result.ok()) {
        json.Null("fatal");
    } else {
        JsonDiagnostic(json, *result.fatal(), "fatal");
    }
    json.EndObject();
    return json.Take();
}

int
ExitStatus(const mmd::Result<Document>& result)
{
    if (!result.ok()) {
        return 2;
    }
    for (const mmd::Diagnostic& d : result.diagnostics()) {
        if (d.severity == mmd::Severity::Error) {
            return 1;
        }
    }
    return 0;
}

} // namespace mmdinspect
