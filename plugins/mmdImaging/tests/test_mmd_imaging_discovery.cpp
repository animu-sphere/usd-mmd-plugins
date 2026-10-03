// SPDX-License-Identifier: Apache-2.0
//
// mmdImaging discovery (docs/design/MATERIAL_POLICY.md §12.1): the plugin is
// found and its adapter constructed from `plugInfo.json` alone.
//
// Nothing here links the plugin or registers anything: the session names the
// staged `plugInfo.json` on PXR_PLUGINPATH_NAME, as a runtime composition does,
// and the suite asks OpenUSD what it found, in the order a Hydra host meets it:
//
//   * PlugRegistry: a plugin named `MmdImaging` is registered and not loaded,
//     and it declares exactly one adapter type, derived from
//     `UsdImagingAPISchemaAdapter` and naming the registered `MmdMaterialAPI`;
//   * UsdImaging's adapter registry: no other plugin in the session claims
//     `MmdMaterialAPI`, and the adapter is known before the library is loaded;
//   * construction: the registry builds the adapter, of the declared type,
//     which is what loads the library.

#include "pxr/pxr.h"

#include "pxr/base/js/value.h"
#include "pxr/base/plug/plugin.h"
#include "pxr/base/plug/registry.h"
#include "pxr/base/tf/token.h"
#include "pxr/base/tf/type.h"
#include "pxr/usd/usd/schemaRegistry.h"
#include "pxr/usdImaging/usdImaging/adapterRegistry.h"
#include "pxr/usdImaging/usdImaging/apiSchemaAdapter.h"

#include <cassert>
#include <cstdio>
#include <set>
#include <string>

PXR_NAMESPACE_USING_DIRECTIVE

namespace {

const char* const kPlugin = "MmdImaging";
const char* const kSchema = "MmdMaterialAPI";
const char* const kAdapter = "UsdMmdImagingMaterialAPIAdapter";

std::string
ApiSchemaName(const TfType& type)
{
    const JsValue name = PlugRegistry::GetInstance().GetDataFromPluginMetaData(
        type, "apiSchemaName");
    return name.IsString() ? name.GetString() : std::string();
}

PlugPluginPtr
ThePlugin()
{
    const PlugPluginPtr plugin =
        PlugRegistry::GetInstance().GetPluginWithName(kPlugin);
    if (!plugin) {
        std::fprintf(stderr, "no plugin named %s: is its plugInfo.json on "
                             "PXR_PLUGINPATH_NAME?\n", kPlugin);
    }
    assert(plugin && "mmdImaging is not discovered");
    return plugin;
}

void
TestThePluginDeclaresOneAdapter()
{
    const PlugPluginPtr plugin = ThePlugin();
    assert(!plugin->IsLoaded() && "something loaded the library before "
                                  "anything asked for an adapter");

    std::set<TfType> adapters;
    PlugRegistry::GetAllDerivedTypes<UsdImagingAPISchemaAdapter>(&adapters);

    std::set<std::string> declared;
    int claims = 0;
    for (const TfType& type : adapters) {
        const std::string schema = ApiSchemaName(type);
        if (schema == kSchema) {
            ++claims;
        }
        if (PlugRegistry::GetInstance().GetPluginForType(type) == plugin) {
            declared.insert(type.GetTypeName() + " -> " + schema);
        }
    }
    for (const std::string& entry : declared) {
        std::printf("  %s\n", entry.c_str());
    }
    assert(declared == std::set<std::string>{std::string(kAdapter) + " -> " + kSchema} &&
           "the plugin does not declare exactly the MmdMaterialAPI adapter");
    if (claims != 1) {
        std::fprintf(stderr, "%d adapters claim %s\n", claims, kSchema);
    }
    assert(claims == 1 && "another plugin claims MmdMaterialAPI");

    // It names a schema the session has registered: mmdSchema's, which this
    // plugin does not link.
    const TfType schemaType =
        UsdSchemaRegistry::GetTypeFromSchemaTypeName(TfToken(kSchema));
    assert(!schemaType.IsUnknown() && "MmdMaterialAPI is not registered");
    assert(UsdSchemaRegistry::IsAppliedAPISchema(schemaType));
    assert(!UsdSchemaRegistry::IsMultipleApplyAPISchema(schemaType));
}

void
TestUsdImagingConstructsTheAdapter()
{
    UsdImagingAdapterRegistry& registry = UsdImagingAdapterRegistry::GetInstance();
    assert(registry.HasAPISchemaAdapter(TfToken(kSchema)) &&
           "UsdImaging knows no adapter for MmdMaterialAPI");
    // Knowing it costs nothing: the registry read plugInfo.json only.
    assert(!ThePlugin()->IsLoaded() &&
           "the adapter registry loaded the library to learn its keys");

    const UsdImagingAPISchemaAdapterSharedPtr adapter =
        registry.ConstructAPISchemaAdapter(TfToken(kSchema));
    assert(adapter && "UsdImaging did not construct the adapter");
    const TfType built = TfType::Find(*adapter);
    if (built.GetTypeName() != kAdapter) {
        std::fprintf(stderr, "built a %s, not a %s\n",
                     built.GetTypeName().c_str(), kAdapter);
    }
    assert(built.GetTypeName() == kAdapter && "the adapter is not the declared type");
    assert(ThePlugin()->IsLoaded() && "constructing an adapter loads the library");
    // Where it was loaded from, for a caller that has to know which copy
    // answered: the product smoke's run against the installed product.
    std::printf("loaded: %s\n", ThePlugin()->GetPath().c_str());
}

} // namespace

int
main()
{
    TestThePluginDeclaresOneAdapter();
    TestUsdImagingConstructsTheAdapter();
    std::printf("mmdImaging_discovery: passed\n");
    return 0;
}
