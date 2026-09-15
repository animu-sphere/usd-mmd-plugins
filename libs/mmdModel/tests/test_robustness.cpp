// SPDX-License-Identifier: Apache-2.0
//
// Canonicalization is total over every document the parser can return. The
// parser's own robustness suite and fuzzer establish which documents those
// are -- every index names an element or is none, faces name vertices,
// material face counts fit -- and within that, anything goes: parents that
// cycle or name themselves, weights that are negative, NaN or huge, names
// that are all punctuation or padded with U+0000, texture paths built from
// the pieces that make a path unsafe. Each generated document is
// canonicalized twice and checked against the promises CanonicalDocument.h
// makes. The generator is a fixed-seed PRNG with its own arithmetic, so the
// run is the same on every platform.
#include "mmdModel/Canonicalize.h"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <set>
#include <string>
#include <vector>

namespace {

namespace pmx = mmd::pmx;

/// SplitMix64: small, and specified to the bit, unlike the standard
/// library's distributions.
class Random {
public:
    explicit Random(std::uint64_t seed) : _state(seed) {}

    std::uint64_t Next()
    {
        std::uint64_t z = (_state += 0x9E3779B97F4A7C15ull);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }

    /// [0, n)
    std::size_t Below(std::size_t n) { return n == 0 ? 0 : static_cast<std::size_t>(Next() % n); }
    bool OneIn(std::size_t n) { return Below(n) == 0; }

    template <class T>
    const T& Pick(const std::vector<T>& values)
    {
        return values[Below(values.size())];
    }

private:
    std::uint64_t _state;
};

float
WildFloat(Random& r)
{
    static const std::vector<float> special{0.0f, -0.0f, 1.0f, -1.0f, 0.5f, 1e-30f, 1e30f,
        -1e30f, std::numeric_limits<float>::max(), std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()};
    if (r.OneIn(4)) {
        return r.Pick(special);
    }
    return static_cast<float>(static_cast<double>(r.Below(2001)) / 100.0 - 10.0);
}

pmx::Vec3
WildVec3(Random& r)
{
    return {WildFloat(r), WildFloat(r), WildFloat(r)};
}

std::string
WildName(Random& r)
{
    static const std::vector<std::string> pieces{"", "a", "Z", "9", "_", " ", "-", "ab",
        "左腕", "ＩＫ", "・", std::string("\0", 1), "Arm", "arm", "ARM", "bone_0001",
        "material_0000", "x_1", std::string(70, 'q')};
    std::string name;
    const std::size_t n = r.Below(4);
    for (std::size_t i = 0; i < n; ++i) {
        name += r.Pick(pieces);
    }
    return name;
}

std::string
WildPath(Random& r)
{
    static const std::vector<std::string> pieces{"tex", "髪.png", "..", ".", "/", "\\", "C:",
        "http:", "a", "b.bmp", "//", std::string("\0", 1), " ", "~", "\t", "\x7F",
        "\xC2\x85", "\xC2\xA0"};
    std::string path;
    const std::size_t n = r.Below(6);
    for (std::size_t i = 0; i < n; ++i) {
        path += r.Pick(pieces);
    }
    return path;
}

/// An index into a table of `size`, or none.
std::int32_t
Index(Random& r, std::size_t size)
{
    if (size == 0 || r.OneIn(5)) {
        return pmx::kNoIndex;
    }
    return static_cast<std::int32_t>(r.Below(size));
}

pmx::Document
Generate(Random& r)
{
    pmx::Document doc;
    doc.header.version = r.OneIn(2) ? pmx::Version::V2_0 : pmx::Version::V2_1;
    doc.header.globals.additionalVec4Count = static_cast<std::uint8_t>(r.Below(5));
    doc.model = {WildName(r), WildName(r), WildName(r), WildName(r)};

    const std::size_t bones = r.OneIn(6) ? 0 : r.Below(12);
    for (std::size_t i = 0; i < bones; ++i) {
        pmx::Bone b;
        b.name = WildName(r);
        b.englishName = WildName(r);
        b.position = WildVec3(r);
        // Anything the parser can leave, cycles and self-parents included.
        b.parent = Index(r, bones);
        doc.bones.push_back(b);
    }

    const std::size_t vertices = r.Below(10);
    for (std::size_t i = 0; i < vertices; ++i) {
        pmx::Vertex v;
        v.position = WildVec3(r);
        v.normal = WildVec3(r);
        v.uv = {WildFloat(r), WildFloat(r)};
        for (auto& a : v.additionalVec4) {
            a = {WildFloat(r), WildFloat(r), WildFloat(r), WildFloat(r)};
        }
        v.deform.type = static_cast<pmx::DeformType>(r.Below(5));
        for (auto& b : v.deform.bones) {
            b = Index(r, bones);
        }
        for (auto& w : v.deform.weights) {
            w = WildFloat(r);
        }
        v.deform.sdefC = WildVec3(r);
        v.deform.sdefR0 = WildVec3(r);
        v.deform.sdefR1 = WildVec3(r);
        v.edgeScale = WildFloat(r);
        doc.vertices.push_back(v);
    }

    const std::size_t triangles = vertices == 0 ? 0 : r.Below(8);
    for (std::size_t i = 0; i < 3 * triangles; ++i) {
        doc.faces.push_back(static_cast<std::uint32_t>(r.Below(vertices)));
    }

    const std::size_t textures = r.Below(5);
    for (std::size_t i = 0; i < textures; ++i) {
        doc.textures.push_back(WildPath(r));
    }

    // Face counts that fit the table, as the parser guarantees, falling short
    // of it at random.
    std::size_t left = triangles;
    const std::size_t materials = r.Below(5);
    for (std::size_t i = 0; i < materials; ++i) {
        pmx::Material m;
        m.name = WildName(r);
        m.englishName = WildName(r);
        m.flags = static_cast<std::uint8_t>(r.Below(256));
        m.texture = Index(r, textures);
        m.sphereTexture = Index(r, textures);
        m.toonReference = r.OneIn(2) ? pmx::ToonReference::Texture : pmx::ToonReference::Shared;
        m.toonTexture = Index(r, textures);
        const std::size_t faces = i + 1 == materials && r.OneIn(2) ? left : r.Below(left + 1);
        m.faceCount = static_cast<std::int32_t>(3 * faces);
        left -= faces;
        doc.materials.push_back(m);
    }
    doc.softBodies.resize(r.Below(2));
    return doc;
}

bool
IsIdentifier(const std::string& id)
{
    if (id.empty() || (id[0] >= '0' && id[0] <= '9')) {
        return false;
    }
    for (char c : id) {
        const bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')
            || c == '_';
        if (!ok) {
            return false;
        }
    }
    return true;
}

std::string
Lower(std::string s)
{
    for (char& c : s) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }
    return s;
}

/// The first promise of CanonicalDocument.h the result breaks, or "".
std::string
Violation(const pmx::Document& doc, const mmd::CanonicalDocument& c)
{
    const std::size_t nv = doc.vertices.size();
    const std::size_t nb = doc.bones.size();
    const mmd::Mesh& mesh = c.mesh;

    // Skeleton: a permutation, parents first, identifiers unique and valid.
    if (c.skeleton.bones.size() != nb || c.skeleton.jointOfSourceBone.size() != nb) {
        return "the skeleton is not one joint per bone";
    }
    std::set<std::string> ids;
    std::set<std::string> paths;
    std::vector<bool> seen(nb, false);
    for (std::size_t j = 0; j < nb; ++j) {
        const mmd::Bone& bone = c.skeleton.bones[j];
        if (bone.sourceIndex >= nb || seen[bone.sourceIndex]) {
            return "the joint order is not a permutation";
        }
        seen[bone.sourceIndex] = true;
        if (c.skeleton.jointOfSourceBone[bone.sourceIndex] != static_cast<std::int32_t>(j)) {
            return "jointOfSourceBone disagrees with the order";
        }
        if (bone.parent != mmd::kNone && (bone.parent < 0 || bone.parent >= static_cast<std::int32_t>(j))) {
            return "a joint's parent does not precede it";
        }
        if (!IsIdentifier(bone.name.stableId) || !ids.insert(Lower(bone.name.stableId)).second) {
            return "a bone identifier is invalid or not unique: " + bone.name.stableId;
        }
        if (!paths.insert(bone.jointPath).second) {
            return "two joints share a path";
        }
        const std::string expected = bone.parent == mmd::kNone
            ? bone.name.stableId
            : c.skeleton.bones[static_cast<std::size_t>(bone.parent)].jointPath + "/"
                + bone.name.stableId;
        if (bone.jointPath != expected) {
            return "a joint path is not its parent's path and its identifier";
        }
    }

    // Mesh: every array sized to the vertices, faces in range.
    if (mesh.points.size() != nv || mesh.normals.size() != nv || mesh.st.size() != nv
        || mesh.edgeScale.size() != nv) {
        return "a per-vertex array is not sized to the vertices";
    }
    for (std::size_t k = 0; k < 4; ++k) {
        if (mesh.additionalUv[k].size() != (k < mesh.additionalUvCount ? nv : 0)) {
            return "an additional UV channel has the wrong size";
        }
    }
    if (mesh.faceVertexIndices.size() != doc.faces.size()) {
        return "the face table changed size";
    }
    for (std::int32_t v : mesh.faceVertexIndices) {
        if (v < 0 || static_cast<std::size_t>(v) >= nv) {
            return "a face names no vertex";
        }
    }

    // Skinning: present exactly with bones, joints in range, weights summing to 1.
    const std::size_t n = mesh.influencesPerVertex;
    if (nb == 0) {
        if (n != 0 || !mesh.jointIndices.empty() || !mesh.deformTypes.empty()) {
            return "a model without bones is skinned";
        }
    } else if (nv > 0) {
        if ((n != 1 && n != 2 && n != 4) || mesh.jointIndices.size() != nv * n
            || mesh.jointWeights.size() != nv * n || mesh.deformTypes.size() != nv) {
            return "the skinning arrays are not N per vertex";
        }
        for (std::size_t v = 0; v < nv; ++v) {
            double sum = 0.0;
            for (std::size_t k = 0; k < n; ++k) {
                const std::int32_t joint = mesh.jointIndices[v * n + k];
                const float w = mesh.jointWeights[v * n + k];
                if (joint < 0 || static_cast<std::size_t>(joint) >= nb) {
                    return "a joint index is out of range";
                }
                if (!std::isfinite(w) || w < 0.0f) {
                    return "a weight is negative or not finite";
                }
                sum += w;
            }
            if (std::abs(sum - 1.0) > 1e-4) {
                return "a vertex's weights do not sum to 1";
            }
        }
        const bool sdef = mesh.sdefVertexCount > 0;
        if (mesh.sdefC.size() != (sdef ? nv : 0)) {
            return "the SDEF arrays are the wrong size";
        }
    }

    // Materials: contiguous ranges within the faces, unique identifiers,
    // texture slots that name a texture.
    std::set<std::string> materialIds;
    std::size_t next = 0;
    for (const mmd::Material& m : c.materials) {
        if (m.firstFace != next || m.firstFace + m.faceCount > mesh.FaceCount()) {
            return "a material's face range is not the next one, or leaves the faces";
        }
        next += m.faceCount;
        if (!IsIdentifier(m.name.stableId) || !materialIds.insert(Lower(m.name.stableId)).second) {
            return "a material identifier is invalid or not unique";
        }
        for (std::int32_t t : {m.texture, m.sphereTexture, m.toonTexture}) {
            if (t != mmd::kNone && (t < 0 || static_cast<std::size_t>(t) >= c.textures.size())) {
                return "a texture slot names no texture";
            }
        }
    }
    if (mesh.materialsCoverFaces != (next == mesh.FaceCount())) {
        return "materialsCoverFaces disagrees with the ranges";
    }

    // Textures: an authored path is anchored, relative, and stays inside.
    if (c.textures.size() != doc.textures.size()) {
        return "the texture table changed size";
    }
    for (std::size_t i = 0; i < c.textures.size(); ++i) {
        const std::string& p = c.textures[i].assetPath;
        if (c.textures[i].sourcePath != doc.textures[i]) {
            return "a texture's source path is not verbatim";
        }
        if (p.empty()) {
            continue;
        }
        if (p.rfind("./", 0) != 0 || p.find('\\') != std::string::npos
            || p.find("//") != std::string::npos || p == "./.." || p.rfind("./../", 0) == 0
            || p.find("/./") != std::string::npos) {
            return "an authored texture path is not normalized: " + p;
        }
        // No control character: SdfAssetPath would refuse the path.
        for (std::size_t k = 0; k < p.size(); ++k) {
            const auto c = static_cast<unsigned char>(p[k]);
            const bool c1 = c == 0xC2 && k + 1 < p.size()
                && static_cast<unsigned char>(p[k + 1]) >= 0x80
                && static_cast<unsigned char>(p[k + 1]) <= 0x9F;
            if (c < 0x20 || c == 0x7F || c1) {
                return "an authored texture path holds a control character";
            }
        }
    }
    return {};
}

/// Every value of a canonical document, as bytes: the same document must
/// produce the same bits, which operator== cannot say once a NaN is present.
class Fingerprint {
public:
    explicit Fingerprint(const mmd::CanonicalDocument& c)
    {
        const mmd::Mesh& m = c.mesh;
        Add(c.metadata.name, c.metadata.englishName, c.metadata.comment, c.metadata.englishComment);
        for (const mmd::Texture& t : c.textures) {
            Add(t.sourcePath, t.assetPath);
        }
        Raw(m.points);
        Raw(m.normals);
        Raw(m.st);
        for (const auto& channel : m.additionalUv) {
            Raw(channel);
        }
        Raw(m.edgeScale);
        Raw(m.faceVertexIndices);
        Raw(m.deformTypes);
        Raw(m.jointIndices);
        Raw(m.jointWeights);
        Raw(m.sdefC);
        Raw(m.sdefR0);
        Raw(m.sdefR1);
        Add(std::to_string(m.influencesPerVertex), m.doubleSided ? "d" : "s",
            m.materialsCoverFaces ? "p" : "n");
        for (const mmd::Material& mat : c.materials) {
            Add(mat.name.source, mat.name.english, mat.name.stableId,
                std::to_string(mat.firstFace) + "," + std::to_string(mat.faceCount) + ","
                    + std::to_string(mat.texture) + "," + std::to_string(mat.sphereTexture) + ","
                    + std::to_string(mat.toonTexture));
        }
        for (const mmd::Bone& b : c.skeleton.bones) {
            Add(b.name.source, b.jointPath, std::to_string(b.sourceIndex),
                std::to_string(b.parent));
            Raw(std::vector<mmd::Double3>{b.position, b.localTranslation});
        }
    }

    bool operator==(const Fingerprint&) const = default;

private:
    template <class... Strings>
    void Add(const Strings&... strings)
    {
        ((_bytes += std::string(strings) + '\x1F'), ...);
    }

    template <class T>
    void Raw(const std::vector<T>& values)
    {
        const char* data = reinterpret_cast<const char*>(values.data());
        _bytes.append(data, values.size() * sizeof(T));
        _bytes += '\x1E';
    }

    std::string _bytes;
};

}  // namespace

int
main()
{
    Random random(0x6D6D644D6F64656Cull);  // "mmdModel"
    constexpr std::size_t kDocuments = 20000;
    std::size_t diagnostics = 0;
    for (std::size_t i = 0; i < kDocuments; ++i) {
        const pmx::Document doc = Generate(random);
        const auto first = mmd::Canonicalize(doc);
        assert(first.ok());
        for (const mmd::Diagnostic& d : first.diagnostics()) {
            assert(d.recoverable);
        }
        diagnostics += first.diagnostics().size();
        const std::string violation = Violation(doc, first.value());
        if (!violation.empty()) {
            std::fprintf(stderr, "document %zu: %s\n", i, violation.c_str());
        }
        assert(violation.empty());
        // The same document, the same canonical model, bit for bit.
        const auto second = mmd::Canonicalize(doc);
        assert(Fingerprint(second.value()) == Fingerprint(first.value()));
        assert(second.diagnostics().size() == first.diagnostics().size());
    }
    // The run is only meaningful if the generator reaches the repairs.
    assert(diagnostics > kDocuments);
    std::printf("robustness: %zu documents, %zu diagnostics\n", kDocuments, diagnostics);
    return 0;
}
