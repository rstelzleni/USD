//
// Copyright 2025 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#ifndef PXR_EXEC_EXEC_COMPILATION_STATE_H
#define PXR_EXEC_EXEC_COMPILATION_STATE_H

#include "pxr/pxr.h"

#include "pxr/exec/exec/api.h"

#include "pxr/exec/exec/compilerTaskSync.h"

PXR_NAMESPACE_OPEN_SCOPE

class EsfStage;
class Exec_CompilationTask;
class Exec_CompiledOutputCache;
class VdfNetwork;

/// Data shared between all compilation tasks.
/// 
/// We construct an instance of this class at the beginning of a round of
/// compilation and then pass along a reference to this instance to all
/// compilation tasks. This prevents bloating the size of every task with this
/// commonly used data.
/// 
class Exec_CompilationState
{
public:
    Exec_CompilationState(
        const EsfStage &stage,
        VdfNetwork * const network,
        Exec_CompiledOutputCache * const compiledOutputs) :
        _stage(stage),
        _network(network),
        _compiledOutputs(compiledOutputs)
    {}

    /// The scene adapter stage.
    const EsfStage &GetStage() const {
        return _stage;
    }

    /// The Vdf network populated by compilation.
    VdfNetwork *GetNetwork() {
        return _network;
    }

    /// This cache stores the compiled outputs.
    Exec_CompiledOutputCache *GetCompiledOutputCache() {
        return _compiledOutputs;
    }

    class OutputTasksAccess {
        friend class Exec_CompilationTask;

        static Exec_CompilerTaskSync &_Get(Exec_CompilationState *state) {
            return state->_outputTasks;
        }
    };

private:

    const EsfStage &_stage;
    Exec_CompilerTaskSync _outputTasks;
    VdfNetwork * const _network;
    Exec_CompiledOutputCache * const _compiledOutputs;

};

PXR_NAMESPACE_CLOSE_SCOPE

#endif
