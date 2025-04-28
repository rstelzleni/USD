#
# Copyright 2023 Pixar
#
# Licensed under the terms set forth in the LICENSE.txt file available at
# https://openusd.org/license.
#
[
    dict(
        SCHEMA_NAME = 'ALL_SCHEMAS',
        LIBRARY_PATH = 'pxr/usdImaging/usdImaging',
    ),        

    #--------------------------------------------------------------------------
    # usdImaging/usdPrimInfo
    dict(
        SCHEMA_NAME = 'UsdPrimInfo',
        SCHEMA_TOKEN = '__usdPrimInfo',
        ADD_DEFAULT_LOCATOR = True,
        MEMBERS = [
            ('specifier', T_TOKEN, {}),
            ('typeName', T_TOKEN, {}),
            ('isLoaded', T_BOOL, {}),
            # Skipping isModel and isGroup, which can be inferred from 'kind'.
            ('apiSchemas', T_TOKENARRAY, {}),
            ('kind', T_TOKEN, {}),
            # XXX Add variantSets. Is it a token array, or a container of token
            #     to token array?
            ('niPrototypePath', T_PATH, dict(ADD_LOCATOR=True)),
            ('isNiPrototype', T_BOOL, {}),
            ('piPropagatedPrototypes', T_CONTAINER, {}),

        ],
        STATIC_TOKEN_DATASOURCE_BUILDERS = [
            ('specifier', ['def', 'over', '(class_, "class")']),
        ],
    ),

    #--------------------------------------------------------------------------
    # usdImaging/model - corresponds to UsdModelAPI
    dict(
        SCHEMA_NAME = 'Model',
        SCHEMA_TOKEN = 'model',
        ADD_DEFAULT_LOCATOR = True,
        MEMBERS = [
            ('modelPath', T_PATH, {}),
            ('assetIdentifier', T_ASSETPATH, {}),
            ('assetName', T_STRING, {}),
            ('assetVersion', T_STRING, {}),
        ],
    ),

    #--------------------------------------------------------------------------
    # usdImaging/geomModel - corresponds to UsdGeomModelAPI
    dict(
        SCHEMA_NAME = 'GeomModel',
        SCHEMA_TOKEN = 'geomModel',
        ADD_DEFAULT_LOCATOR = True,
        MEMBERS = [
            ('drawMode', T_TOKEN, dict(ADD_LOCATOR=True)),
            ('applyDrawMode', T_BOOL, {}),
            ('drawModeColor', T_VEC3F, {}),
            ('cardGeometry', T_TOKEN, {}),
            ('cardTextureXPos', T_ASSETPATH, {}),
            ('cardTextureYPos', T_ASSETPATH, {}),
            ('cardTextureZPos', T_ASSETPATH, {}),
            ('cardTextureXNeg', T_ASSETPATH, {}),
            ('cardTextureYNeg', T_ASSETPATH, {}),
            ('cardTextureZNeg', T_ASSETPATH, {}),
        ],
        STATIC_TOKEN_DATASOURCE_BUILDERS = [
            ('drawMode', [
                '(default_, "default")',
                'origin',
                'bounds',
                'cards',
                'inherited']),
            ('cardGeometry', [
                'cross',
                'box',
                'fromTexture']),
        ],
    ),

    #--------------------------------------------------------------------------
    # usdImaging/directMaterialBinding - corresponds to UsdShadeMaterialBindingAPI::DirectBinding
    dict(
        SCHEMA_NAME = 'DirectMaterialBinding',
        SCHEMA_TOKEN = 'directMaterialBinding',
        ADD_DEFAULT_LOCATOR = True,
        MEMBERS = [
            ('materialPath', T_PATH, {}),
            ('bindingStrength', T_TOKEN, {}),
        ],
    ),

    #--------------------------------------------------------------------------
    # usdImaging/collectionMaterialBinding - corresponds to UsdShadeMaterialBindingAPI::CollectionBinding
    dict(
        SCHEMA_NAME = 'CollectionMaterialBinding',
        SCHEMA_TOKEN = 'collectionMaterialBinding',
        ADD_DEFAULT_LOCATOR = True,
        MEMBERS = [
            ('collectionPath', T_PATH, {}),
            ('materialPath', T_PATH, {}),
            ('bindingStrength', T_TOKEN, {}),
        ],
    ),

    #--------------------------------------------------------------------------
    # usdImaging/materialBinding
    dict(
        SCHEMA_NAME = 'MaterialBinding',
        # HdMaterialBinding schema uses the 'materialBinding' token
        # (locator), so we use a different token here.
        SCHEMA_TOKEN = 'usdMaterialBinding',
        DOC = '''The {{ SCHEMA_CLASS_NAME }} specifies a container for a prim's
        material bindings for a particular purpose. Note that only one direct
        binding but any number of collection-based bindings may be declared
        for a given purpose.
        See UsdImagingMaterialBindingsSchema which specifies the purposes and
        their associated bindings.''',
        SCHEMA_INCLUDES =
            ['{{LIBRARY_PATH}}/directMaterialBindingSchema'],
        ADD_DEFAULT_LOCATOR = True,
        MEMBERS = [
            ('directMaterialBinding', 'UsdImagingDirectMaterialBindingSchema', {}),
            ('collectionMaterialBindings', 'UsdImagingCollectionMaterialBindingVectorSchema', {}),
        ],
    ),

    #--------------------------------------------------------------------------
    # usdImaging/materialBindings - corresponds to UsdShadeMaterialBindingAPI
    dict(
        SCHEMA_NAME = 'MaterialBindings',
        # Note: HdMaterialBindings schema uses the 'materialBindings' token
        # (locator), so we use a different token here.
        SCHEMA_TOKEN = 'usdMaterialBindings',
        DOC = '''The {{ SCHEMA_CLASS_NAME }} specifies a container for all the
        material bindings declared on a prim. The material binding purpose
        serves as the key, with the value being a vector of
        UsdImagingMaterialBindingSchema. While one entry (element) would suffice
        for a prim's material bindings opinion, we use a vector for aggregating
        ancestor material bindings to model the inheritance semantics of
        UsdShadeMaterialBindingAPI.''',
        ADD_DEFAULT_LOCATOR = True,
        EXTRA_TOKENS = [
            '(allPurpose, "")',
        ],
    ),

    #--------------------------------------------------------------------------
    # usdImaging/usdRenderSettings
    dict(
        SCHEMA_NAME = 'UsdRenderSettings',
        SCHEMA_TOKEN = '__usdRenderSettings',
        ADD_DEFAULT_LOCATOR = True,
        MEMBERS = [
            ('ALL_MEMBERS', '', dict(ADD_LOCATOR=True)),
            # UsdRenderSettingsBase
            ('resolution', T_VEC2I, {}),
            ('pixelAspectRatio', T_FLOAT, {}),
            ('aspectRatioConformPolicy', T_TOKEN, {}),
            ('dataWindowNDC', T_VEC4F, {}), # XXX T_RANGE2F
            ('disableMotionBlur', T_BOOL, {}),
            # note: instantaneousShutter is deprecated in favor of 
            # disableMotionBlur, so we skip it.
            ('disableDepthOfField', T_BOOL, {}),
            ('camera', T_PATH, {}),

            # UsdRenderSettings
            ('includedPurposes', T_TOKENARRAY, {}),
            ('materialBindingPurposes', T_TOKENARRAY, {}),
            ('renderingColorSpace', T_TOKEN, {}),
            ('products', T_PATHARRAY, {}),

            # note: namespacedSettings isn't in the USD schema.
            ('namespacedSettings', T_CONTAINER, {}),
        ],
    ),

    #--------------------------------------------------------------------------
    # usdImaging/usdRenderProduct
    dict(
        SCHEMA_NAME = 'UsdRenderProduct',
        SCHEMA_TOKEN = '__usdRenderProduct',
        ADD_DEFAULT_LOCATOR = True,
        MEMBERS = [
            # UsdRenderSettingsBase
            ('resolution', T_VEC2I, {}),
            ('pixelAspectRatio', T_FLOAT, {}),
            ('aspectRatioConformPolicy', T_TOKEN, {}),
            ('dataWindowNDC', T_VEC4F, {}), # XXX T_RANGE2F
            ('disableMotionBlur', T_BOOL, {}),
            # note: instantaneousShutter is deprecated in favor of 
            # disableMotionBlur, so we skip it.
            ('disableDepthOfField', T_BOOL, {}),
            ('camera', T_PATH, {}),

            # UsdRenderProduct
            ('productType', T_TOKEN, {}),
            ('productName', T_TOKEN, {}),
            ('orderedVars', T_PATHARRAY, {}),

            # note: namespacedSettings isn't in the USD schema.
            ('namespacedSettings', T_CONTAINER, dict(ADD_LOCATOR=True)),
        ],
    ),

    #--------------------------------------------------------------------------
    # usdImaging/usdRenderVar
    dict(
        SCHEMA_NAME = 'UsdRenderVar',
        SCHEMA_TOKEN = '__usdRenderVar',
        ADD_DEFAULT_LOCATOR = True,
        MEMBERS = [
            # UsdRenderProduct
            ('dataType', T_TOKEN, {}),
            ('sourceName', T_STRING, {}),
            ('sourceType', T_TOKEN, {}),

            # note: namespacedSettings isn't in the USD schema.
            ('namespacedSettings', T_CONTAINER, dict(ADD_LOCATOR=True)),
        ],
    ),
] 
