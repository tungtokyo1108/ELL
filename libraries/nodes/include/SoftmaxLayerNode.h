////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Project:  Embedded Learning Library (ELL)
//  File:     SoftmaxLayerNode.h (nodes)
//  Authors:  Chuck Jacobs
//
////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "BroadcastFunctionNode.h"
#include "NeuralNetworkLayerNode.h"

#include <emitters/include/LLVMUtilities.h>

#include <model/include/IRMapCompiler.h>
#include <model/include/ModelTransformer.h>
#include <model/include/PortElements.h>
#include <model/include/PortMemoryLayout.h>

#include <predictors/neural/include/SoftmaxLayer.h>

#include <string>
#include <type_traits>

namespace ell
{
namespace nodes
{
    /// <summary> A node that wraps a neural net SoftmaxLayer. </summary>
    template <typename ValueType>
    class SoftmaxLayerNode : public NeuralNetworkLayerNode<SoftmaxLayerNode<ValueType>, predictors::neural::SoftmaxLayer<ValueType>, ValueType>
    {
    public:
        using LayerType = predictors::neural::SoftmaxLayer<ValueType>;
        using BaseType = NeuralNetworkLayerNode<SoftmaxLayerNode<ValueType>, predictors::neural::SoftmaxLayer<ValueType>, ValueType>;

        /// @name Input and Output Ports
        /// @{
        using BaseType::input;
        using BaseType::output;
        /// @}

        SoftmaxLayerNode() = default;

        /// <summary> Constructor from a layer. </summary>
        ///
        /// <param name="input"> </param>
        /// <param name="layer"> The bias layer to wrap. </param>
        SoftmaxLayerNode(const model::OutputPort<ValueType>& input, const predictors::neural::SoftmaxLayer<ValueType>& layer);

        /// <summary> Gets the name of this type (for serialization). </summary>
        ///
        /// <returns> The name of this type. </returns>
        static std::string GetTypeName() { return utilities::GetCompositeTypeName<ValueType>("SoftmaxLayerNode"); }

        /// <summary> Gets the name of this type (for serialization). </summary>
        ///
        /// <returns> The name of this type. </returns>
        std::string GetRuntimeTypeName() const override { return GetTypeName(); }

        /// <summary> Indicates if this node is able to compile itself to code. </summary>
        bool IsCompilable(const model::MapCompiler* compiler) const override { return true; }

    protected:
        void Compile(model::IRMapCompiler& compiler, emitters::IRFunctionEmitter& function) override;
        using BaseType::HasState;

    private:
        void Copy(model::ModelTransformer& transformer) const override;

        // Helper for generating nested loops to visit all input/output values
        template <typename FunctionType>
        void EmitComputeDimensionLoop(model::IRMapCompiler& compiler,
                                      emitters::IRFunctionEmitter& function,
                                      size_t dimension,
                                      const model::PortMemoryLayout& inputLayout,
                                      const model::PortMemoryLayout& outputLayout,
                                      emitters::LLVMValue pInput,
                                      emitters::LLVMValue pOutput,
                                      emitters::LLVMValue prevInputDimensionOffset,
                                      emitters::LLVMValue prevOutputDimensionOffset,
                                      FunctionType& f) const;

        // in-place version
        template <typename FunctionType>
        void EmitComputeDimensionLoop(model::IRMapCompiler& compiler,
                                      emitters::IRFunctionEmitter& function,
                                      size_t dimension,
                                      const model::PortMemoryLayout& inputLayout,
                                      emitters::LLVMValue pInput,
                                      emitters::LLVMValue prevInputDimensionOffset,
                                      FunctionType& f) const;
    };
} // namespace nodes
} // namespace ell
