#ifndef OPERATORDESCRIPTOR_H
#define OPERATORDESCRIPTOR_H

#include "../Operator/Operator.h"
#include "../Operator/Activation.h"
#include "../Operator/ActivationDerivative.h"
#include "../Operator/Convolution.h"
#include "../Operator/ConvolutionDerivative.h"
#include "../Operator/CrossEntropyLoss.h"
#include "../Operator/DotProductWithBias.h"
#include "../Operator/DotProductWithBiasDerivative.h"
#include "../Operator/ElementwiseAdd.h"
#include "../Operator/MaxPooling.h"
#include "../Operator/MaxPoolingDerivative.h"
#include "../Operator/SigmoidCrossEntropyLossDerivative.h"
#include "../Operator/SoftmaxLogLoss.h"
#include "../Operator/SoftmaxLogLossDerivative.h"
#include "../Operator/Duplicate.h"
#include "../Operator/Reshape.h"
#include "TensorDescriptor.h"
#include <any>
#include <fstream>
#include "../Context/WorkerMessage.h"
#include <chrono>

namespace FreeWill
{
    //xxx should operator has datatype?

    class Model;
    class Solver;

    typedef std::string OperatorDescriptorHandle;

    class OperatorDescriptor
    {
        friend class Model;
        friend class Solver;

        constexpr static const float topBottomMargin = 20;
        constexpr static const float centerSpace = 40;
        constexpr static const float anchorSpace = 5;
        constexpr static const float anchorHeight = 15;
        constexpr static const float anchorWidth = 80;
        constexpr static const float leftRightMargin = 5;
        constexpr static const float topSpace = 32;
        constexpr static const float bottomSpace = 32;
        constexpr static const float leftSpace = 5;
        constexpr static const float rightSpace = 5;

    private:
        std::string m_name;
        DataType m_dataType;
        OperatorName m_operatorName;
        std::map<std::string, FreeWill::TensorDescriptorHandle> m_inputs;
        std::map<std::string, FreeWill::TensorDescriptorHandle> m_outputs;
        std::map<std::string, std::any> m_parameters;
        std::map<DeviceType, std::vector<std::variant<Operator<DeviceType::GPU_CUDA>*, Operator<DeviceType::CPU_NAIVE>*>>> m_operators;
        std::vector<FreeWill::TensorDescriptorHandle> m_inputsNeedReshape;
        std::vector<FreeWill::TensorDescriptorHandle> m_outputsNeedReshape;

        OperatorDescriptor(const std::string &name, OperatorName operatorName,
                           const std::map<std::string, FreeWill::TensorDescriptorHandle> &inputs,
                           const std::map<std::string, FreeWill::TensorDescriptorHandle> &outputs,
                           const std::map<std::string, std::any> &parameters, DataType dataType = DataType::FLOAT);
        ~OperatorDescriptor();

        void generateSVGDiagram(std::ostream &outputStream, unsigned int &width, unsigned int &height);
        void evaluateSVGDiagramSize(unsigned int &width, unsigned int &height);

        template<DeviceType DeviceUsed>
        bool setInput(Operator<DeviceUsed> *operatorBase, const std::string &inputName, std::map<std::string, FreeWill::TensorDescriptor*> &tensors, int deviceId)
        {
            if (m_inputs.find(inputName) == m_inputs.end())
            {
                return false;
            }

            TensorDescriptor *tensorDescriptor = tensors[m_inputs[inputName].name()];
            TensorBase<DeviceUsed> *tensorBase = tensorDescriptor->getTensorForDevice<DeviceUsed>(deviceId);

            if (m_inputs[inputName].isReshaped())
            {
                Shape newShape = m_inputs[inputName].shape();
                if (! tensorBase->reshape(tensorDescriptor->m_isBatchTensor?(newShape + tensorDescriptor->m_batchSize):newShape ))
                {
                    std::cerr << "Reshape failed for input: " << inputName << " tensor: " << tensorBase->name() << " from: " << tensorBase->shape() << " to: " << m_inputs[inputName].shape() << std::endl;
                    return false;
                }
                m_inputsNeedReshape.push_back(m_inputs[inputName]);
            }

            operatorBase->setInputParameter(inputName, tensorBase);

            return true;
        }

        template<DeviceType DeviceUsed>
        bool setOutput(Operator<DeviceUsed> *operatorBase, const std::string &outputName, std::map<std::string, FreeWill::TensorDescriptor*> &tensors, int deviceId)
        {
            if (m_outputs.find(outputName) == m_outputs.end())
            {
                return false;
            }

            TensorDescriptor *tensorDescriptor = tensors[m_outputs[outputName].name()];
            TensorBase<DeviceUsed> *tensorBase = tensorDescriptor->getTensorForDevice<DeviceUsed>(deviceId);

            if (m_outputs[outputName].isReshaped())
            {
                Shape newShape = m_outputs[outputName].shape();

                if (! tensorBase->reshape(tensorDescriptor->m_isBatchTensor?(newShape + tensorDescriptor->m_batchSize):newShape))
                {
                    std::cerr << "Reshape failed for output: " << outputName << " tensor: " << tensorBase->name() << " from: " << tensorBase->shape() << " to: " << m_outputs[outputName].shape() << std::endl;
                    return false;
                }
                m_outputsNeedReshape.push_back(m_outputs[outputName]);
            }

            operatorBase->setOutputParameter(outputName, tensorBase);

            return true;
        }

        template<DeviceType DeviceUsed>
        Operator<DeviceUsed> *initActivation(std::map<std::string, FreeWill::TensorDescriptor*> &tensors, int deviceId)
        {
            Operator<DeviceUsed> *operatorBase = nullptr;
            if (m_parameters.find("Mode") == m_parameters.end())
            {
                return nullptr;
            }

            FreeWill::ActivationMode mode = std::any_cast<FreeWill::ActivationMode>(m_parameters["Mode"]);
            switch(mode)
            {
            case ActivationMode::SIGMOID:
                switch(m_dataType)
                {
                case DataType::FLOAT:
                    operatorBase = new Activation<ActivationMode::SIGMOID, DeviceUsed, float>(deviceId);
                    break;
                case DataType::DOUBLE:
                    operatorBase = new Activation<ActivationMode::SIGMOID, DeviceUsed, double>(deviceId);
                    break;
                /*case UNSIGNED_INT:
                    operatorBase = new Activation<SIGMOID, DeviceUsed, unsigned int>();
                    break;*/
                case DataType::UNSIGNED_INT:
                    return nullptr;
                }
                break;
            case ActivationMode::RELU:
                switch(m_dataType)
                {
                case DataType::FLOAT:
                    operatorBase = new Activation<ActivationMode::RELU, DeviceUsed, float>(deviceId);
                    break;
                case DataType::DOUBLE:
                    operatorBase = new Activation<ActivationMode::RELU, DeviceUsed, double>(deviceId);
                    break;
                /*case UNSIGNED_INT:
                    operatorBase = new Activation<RELU, DeviceUsed, unsigned int>();
                    break;*/
                case DataType::UNSIGNED_INT:
                    return nullptr;

                }
                break;
            case ActivationMode::TANH:
                switch(m_dataType)
                {
                case DataType::FLOAT:
                    operatorBase = new Activation<ActivationMode::TANH, DeviceUsed, float>(deviceId);
                    break;
                case DataType::DOUBLE:
                    operatorBase = new Activation<ActivationMode::TANH, DeviceUsed, double>(deviceId);
                    break;
                /*case UNSIGNED_INT:
                    operatorBase = new Activation<TANH, DeviceUsed, unsigned int>();
                    break;*/
                case DataType::UNSIGNED_INT:
                    return nullptr;

                }
                break;
            case ActivationMode::CLIPPED_RELU:
                switch(m_dataType)
                {
                case DataType::FLOAT:
                    operatorBase = new Activation<ActivationMode::CLIPPED_RELU, DeviceUsed, float>(deviceId);
                    break;
                case DataType::DOUBLE:
                    operatorBase = new Activation<ActivationMode::CLIPPED_RELU, DeviceUsed, double>(deviceId);
                    break;
                /*case UNSIGNED_INT:
                    operatorBase = new Activation<CLIPPED_RELU, DeviceUsed, unsigned int>();
                    break;*/
                case DataType::UNSIGNED_INT:
                    return nullptr;

                }
                break;
            }

            if (!setInput(operatorBase, "Input", tensors, deviceId) || !setOutput(operatorBase, "Output", tensors, deviceId))
            {
                delete operatorBase;
                operatorBase = nullptr;
            }

            return operatorBase;
        }

        template<DeviceType DeviceUsed>
        Operator<DeviceUsed> *initActivationDerivative(std::map<std::string, FreeWill::TensorDescriptor*> &tensors, int deviceId)
        {
            Operator<DeviceUsed> *operatorBase = nullptr;
            if (m_parameters.find("Mode") == m_parameters.end())
            {
                qDebug() << "activation needs to specify mode";
                return nullptr;
            }

            FreeWill::ActivationMode mode = std::any_cast<FreeWill::ActivationMode>(m_parameters["Mode"]);
            switch(mode)
            {
            case ActivationMode::SIGMOID:
                switch(m_dataType)
                {
                case DataType::FLOAT:
                    operatorBase = new ActivationDerivative<ActivationMode::SIGMOID, DeviceUsed, float>(deviceId);
                    break;
                case DataType::DOUBLE:
                    operatorBase = new ActivationDerivative<ActivationMode::SIGMOID, DeviceUsed, double>(deviceId);
                    break;
                /*case UNSIGNED_INT:
                    operatorBase = new ActivationDerivative<SIGMOID, DeviceUsed, unsigned int>();
                    break;*/
                case DataType::UNSIGNED_INT:
                    return nullptr;
                }
                break;
            case ActivationMode::RELU:
                switch(m_dataType)
                {
                case DataType::FLOAT:
                    operatorBase = new ActivationDerivative<ActivationMode::RELU, DeviceUsed, float>(deviceId);
                    break;
                case DataType::DOUBLE:
                    operatorBase = new ActivationDerivative<ActivationMode::RELU, DeviceUsed, double>(deviceId);
                    break;
                /*case UNSIGNED_INT:
                    operatorBase = new ActivationDerivative<RELU, DeviceUsed, unsigned int>();
                    break;*/
                case DataType::UNSIGNED_INT:
                    return nullptr;
                }
                break;
            case ActivationMode::TANH:
                switch(m_dataType)
                {
                case DataType::FLOAT:
                    operatorBase = new ActivationDerivative<ActivationMode::TANH, DeviceUsed, float>(deviceId);
                    break;
                case DataType::DOUBLE:
                    operatorBase = new ActivationDerivative<ActivationMode::TANH, DeviceUsed, double>(deviceId);
                    break;
                /*case UNSIGNED_INT:
                    operatorBase = new ActivationDerivative<TANH, DeviceUsed, unsigned int>();
                    break;*/
                case DataType::UNSIGNED_INT:
                    return nullptr;
                }
                break;
            case ActivationMode::CLIPPED_RELU:
                switch(m_dataType)
                {
                case DataType::FLOAT:
                    operatorBase = new ActivationDerivative<ActivationMode::CLIPPED_RELU, DeviceUsed, float>(deviceId);
                    break;
                case DataType::DOUBLE:
                    operatorBase = new ActivationDerivative<ActivationMode::CLIPPED_RELU, DeviceUsed, double>(deviceId);
                    break;
                /*case UNSIGNED_INT:
                    operatorBase = new ActivationDerivative<CLIPPED_RELU, DeviceUsed, unsigned int>();
                    break;*/
                case DataType::UNSIGNED_INT:
                    return nullptr;
                }
                break;
            }
            if (!setInput(operatorBase, "Output", tensors, deviceId) ||
                    !setInput(operatorBase, "OutputDelta", tensors, deviceId) ||
                    !setOutput(operatorBase, "InputDelta", tensors, deviceId))
            {
                delete operatorBase;
                return nullptr;
            }

            return operatorBase;
        }

        template<DeviceType DeviceUsed>
        Operator<DeviceUsed> *initConvolution(std::map<std::string, FreeWill::TensorDescriptor*> &tensors, int deviceId)
        {
            Operator<DeviceUsed> *operatorBase = nullptr;

            unsigned int strideX = 1;
            if (m_parameters.find("StrideX") != m_parameters.end())
            {
                strideX = std::any_cast<unsigned int>(m_parameters["StrideX"]);
            }

            unsigned int strideY = 1;
            if (m_parameters.find("StrideX") != m_parameters.end())
            {
                strideY = std::any_cast<unsigned int>(m_parameters["StrideY"]);
            }

            unsigned int zeroPaddingX = 0;
            if (m_parameters.find("ZeroPaddingX") != m_parameters.end())
            {
                zeroPaddingX = std::any_cast<unsigned int>(m_parameters["ZeroPaddingX"]);
            }

            unsigned int zeroPaddingY = 0;
            if (m_parameters.find("ZeroPaddingY") != m_parameters.end())
            {
                zeroPaddingY = std::any_cast<unsigned int>(m_parameters["ZeroPaddingY"]);
            }

            switch(m_dataType)
            {
            case DataType::FLOAT:
                operatorBase = new Convolution<DeviceUsed, float>(strideX,strideY,zeroPaddingX,zeroPaddingY,deviceId);

                break;
            case DataType::DOUBLE:
                operatorBase = new Convolution<DeviceUsed, double>(strideX,strideY,zeroPaddingX,zeroPaddingY,deviceId);

                break;
            /*case UNSIGNED_INT:
                operatorBase = new Convolution<DeviceUsed, unsigned int>();
                break;*/
            case DataType::UNSIGNED_INT:
                return nullptr;

            }

            if (!setInput(operatorBase, "Input", tensors, deviceId) ||
                    !setInput(operatorBase, "FeatureMap", tensors, deviceId) ||
                    !setInput(operatorBase, "Bias", tensors, deviceId) ||
                    !setOutput(operatorBase, "Output", tensors, deviceId))
            {
                delete operatorBase;
                return nullptr;
            }


            return operatorBase;
        }

        template<DeviceType DeviceUsed>
        Operator<DeviceUsed> *initConvolutionDerivative(std::map<std::string, FreeWill::TensorDescriptor*> &tensors, int deviceId)
        {
            Operator<DeviceUsed> *operatorBase = nullptr;

            unsigned int strideX = 1;
            if (m_parameters.find("StrideX") != m_parameters.end())
            {
                strideX = std::any_cast<unsigned int>(m_parameters["StrideX"]);
            }

            unsigned int strideY = 1;
            if (m_parameters.find("StrideX") != m_parameters.end())
            {
                strideY = std::any_cast<unsigned int>(m_parameters["StrideY"]);
            }

            unsigned int zeroPaddingX = 0;
            if (m_parameters.find("ZeroPaddingX") != m_parameters.end())
            {
                zeroPaddingX = std::any_cast<unsigned int>(m_parameters["ZeroPaddingX"]);
            }

            unsigned int zeroPaddingY = 0;
            if (m_parameters.find("ZeroPaddingY") != m_parameters.end())
            {
                zeroPaddingY = std::any_cast<unsigned int>(m_parameters["ZeroPaddingY"]);
            }

            switch(m_dataType)
            {
            case DataType::FLOAT:
                operatorBase = new ConvolutionDerivative<DeviceUsed, float>(strideX,strideY,zeroPaddingX,zeroPaddingY,deviceId);
                break;
            case DataType::DOUBLE:
                operatorBase = new ConvolutionDerivative<DeviceUsed, double>(strideX,strideY,zeroPaddingX,zeroPaddingY,deviceId);
                break;
            /*case UNSIGNED_INT:
                operatorBase = new ConvolutionDerivative<DeviceUsed, unsigned int>();
                break;*/
            case DataType::UNSIGNED_INT:
                return nullptr;

            }

            if (!setInput(operatorBase, "PrevActivation", tensors, deviceId) ||
                    !setInput(operatorBase, "FeatureMap", tensors, deviceId) ||
                    !setInput(operatorBase, "OutputGrad", tensors, deviceId) ||
                    !setOutput(operatorBase, "FeatureMapGrad", tensors, deviceId) ||
                    !setOutput(operatorBase, "BiasGrad", tensors, deviceId) ||
                    !setOutput(operatorBase, "InputGrad", tensors, deviceId))
            {
                delete operatorBase;
                return nullptr;
            }

            return operatorBase;
        }

        template<DeviceType DeviceUsed>
        Operator<DeviceUsed> *initCrossEntropyLoss(std::map<std::string, FreeWill::TensorDescriptor*> &tensors, int deviceId)
        {
            Operator<DeviceUsed> *operatorBase = nullptr;

            switch(m_dataType)
            {
            case DataType::FLOAT:
                operatorBase = new CrossEntropyLoss<DeviceUsed, float>(deviceId);
                break;
            case DataType::DOUBLE:
                operatorBase = new CrossEntropyLoss<DeviceUsed, double>(deviceId);
                break;
            /*case UNSIGNED_INT:
                operatorBase = new CrossEntropyLoss<DeviceUsed, unsigned int>();
                break;*/
            case DataType::UNSIGNED_INT:
                return nullptr;
            }

            if (!setInput(operatorBase, "Input", tensors, deviceId) ||
                    !setInput(operatorBase, "Label", tensors, deviceId) ||
                    !setOutput(operatorBase, "Cost", tensors, deviceId))
            {
                delete operatorBase;
                return nullptr;
            }


            return operatorBase;
        }


        template<DeviceType DeviceUsed>
        Operator<DeviceUsed> *initDotProductWithBias(std::map<std::string, FreeWill::TensorDescriptor*> &tensors, int deviceId)
        {
            Operator<DeviceUsed> *operatorBase = nullptr;

            bool hasBias = true;
            if (m_parameters.find("HasBias") != m_parameters.end())
            {
                hasBias = std::any_cast<bool>(m_parameters["HasBias"]);
            }

            switch(m_dataType)
            {
            case DataType::FLOAT:
                operatorBase = new DotProductWithBias<DeviceUsed, float>(hasBias, deviceId);

                break;
            case DataType::DOUBLE:
                operatorBase = new DotProductWithBias<DeviceUsed, double>(hasBias, deviceId);
                break;
            /*case UNSIGNED_INT:
                operatorBase = new DotProductWithBias<DeviceUsed, unsigned int>();
                break;*/
            case DataType::UNSIGNED_INT:
                return nullptr;
            }

            if (!setInput(operatorBase, "Input", tensors, deviceId) ||
                    !setInput(operatorBase, "Weight", tensors, deviceId) ||
                    !setInput(operatorBase, "Bias", tensors, deviceId) ||
                    !setOutput(operatorBase, "Output", tensors, deviceId))
            {
                delete operatorBase;
                return nullptr;
            }

            return operatorBase;

        }

        template<DeviceType DeviceUsed>
        Operator<DeviceUsed> *initDotProductWithBiasDerivative(std::map<std::string, FreeWill::TensorDescriptor*> &tensors, int deviceId)
        {
            Operator<DeviceUsed> *operatorBase = nullptr;

            bool hasBias = true;
            if (m_parameters.find("HasBias") != m_parameters.end())
            {
                hasBias = std::any_cast<bool>(m_parameters["HasBias"]);
            }

            switch(m_dataType)
            {
            case DataType::FLOAT:
                operatorBase = new DotProductWithBiasDerivative<DeviceUsed, float>(hasBias, deviceId);
                break;
            case DataType::DOUBLE:
                operatorBase = new DotProductWithBiasDerivative<DeviceUsed, double>(hasBias, deviceId);
                break;
            /*case UNSIGNED_INT:
                operatorBase = new DotProductWithBiasDerivative<DeviceUsed, unsigned int>();

                break;*/
            case DataType::UNSIGNED_INT:
                return nullptr;
            }

            if (!setInput(operatorBase, "InputActivation", tensors, deviceId) ||
                    !setInput(operatorBase, "OutputDelta", tensors, deviceId) ||
                    !setInput(operatorBase, "Weight", tensors, deviceId) ||
                    !setOutput(operatorBase, "WeightGrad", tensors, deviceId) ||
                    !setOutput(operatorBase, "BiasGrad", tensors, deviceId) ||
                    !setOutput(operatorBase, "InputDelta", tensors, deviceId))
            {
                delete operatorBase;
                return nullptr;
            }

            return operatorBase;
        }

        template<DeviceType DeviceUsed>
        Operator<DeviceUsed> *initElementwiseAdd(std::map<std::string, FreeWill::TensorDescriptor*> &tensors, int deviceId)
        {
            Operator<DeviceUsed> *operatorBase = nullptr;

            float rate = 1.0;

            if (m_parameters.find("Rate") != m_parameters.end())
            {
                rate = std::any_cast<float>(m_parameters["Rate"]);
            }

            switch(m_dataType)
            {
            case DataType::FLOAT:
                operatorBase = new ElementwiseAdd<DeviceUsed, float>(rate, deviceId);
                break;
            case DataType::DOUBLE:
                operatorBase = new ElementwiseAdd<DeviceUsed, double>(rate, deviceId);
                break;
            /*case UNSIGNED_INT:
                operatorBase = new ElementwiseAdd<DeviceUsed, unsigned int>();
                break;*/
            case DataType::UNSIGNED_INT:
                return nullptr;
            }

            if (!setInput(operatorBase, "OperandA", tensors, deviceId) ||
                    !setInput(operatorBase, "OperandB", tensors, deviceId) ||
                    !setOutput(operatorBase, "Result", tensors, deviceId))
            {
                delete operatorBase;
                return nullptr;
            }

            return operatorBase;
        }

        template<DeviceType DeviceUsed>
        Operator<DeviceUsed> *initMaxPooling(std::map<std::string, FreeWill::TensorDescriptor*> &tensors, int deviceId)
        {
            Operator<DeviceUsed> *operatorBase = nullptr;

            switch(m_dataType)
            {
            case DataType::FLOAT:
                operatorBase = new MaxPooling<DeviceUsed, float>(deviceId);
                break;
            case DataType::DOUBLE:
                operatorBase = new MaxPooling<DeviceUsed, double>(deviceId);
                break;
            /*case UNSIGNED_INT:
                operatorBase = new MaxPooling<DeviceUsed, unsigned int>();
                break;*/
            case DataType::UNSIGNED_INT:
                return nullptr;
            }

            if (!setInput(operatorBase, "Input", tensors, deviceId) ||
                    !setOutput(operatorBase, "Output", tensors, deviceId) ||
                    !setOutput(operatorBase, "SwitchX", tensors, deviceId) ||
                    !setOutput(operatorBase, "SwitchY", tensors, deviceId))
            {
                delete operatorBase;
                return nullptr;
            }

            return operatorBase;

        }

        template<DeviceType DeviceUsed>
        Operator<DeviceUsed> *initMaxPoolingDerivative(std::map<std::string, FreeWill::TensorDescriptor*> &tensors, int deviceId)
        {
            Operator<DeviceUsed> *operatorBase = nullptr;

            switch(m_dataType)
            {
            case DataType::FLOAT:
                operatorBase = new MaxPoolingDerivative<DeviceUsed, float>(deviceId);
                break;
            case DataType::DOUBLE:
                operatorBase = new MaxPoolingDerivative<DeviceUsed, double>(deviceId);
                break;
            /*case UNSIGNED_INT:
                operatorBase = new MaxPoolingDerivative<DeviceUsed, unsigned int>();
                break;*/
            case DataType::UNSIGNED_INT:
                return nullptr;
            }

            if constexpr (DeviceUsed == FreeWill::DeviceType::CPU_NAIVE)
            {
                if (!setInput(operatorBase, "OutputGrad", tensors, deviceId) ||
                    !setInput(operatorBase, "SwitchX", tensors, deviceId) ||
                    !setInput(operatorBase, "SwitchY", tensors, deviceId) ||
                    !setOutput(operatorBase, "InputGrad", tensors, deviceId))
                {
                    delete operatorBase;
                    return nullptr;
                }
            }
            else if constexpr (DeviceUsed == FreeWill::DeviceType::GPU_CUDA)
            {
                if (!setInput(operatorBase, "Output", tensors, deviceId) ||
                    !setInput(operatorBase, "OutputGrad", tensors, deviceId) ||
                    !setInput(operatorBase, "Input", tensors, deviceId) ||
                    !setOutput(operatorBase, "InputGrad", tensors, deviceId))
                {
                    delete operatorBase;
                    return nullptr;
                }
            }

            return operatorBase;
        }

        template<DeviceType DeviceUsed>
        Operator<DeviceUsed> *initSigmoidCrossEntropyLossDerivative(std::map<std::string, FreeWill::TensorDescriptor*> &tensors, int deviceId)
        {
            Operator<DeviceUsed> *operatorBase = nullptr;

            switch(m_dataType)
            {
            case DataType::FLOAT:
                operatorBase = new SigmoidCrossEntropyLossDerivative<DeviceUsed, float>(deviceId);
                break;
            case DataType::DOUBLE:
                operatorBase = new SigmoidCrossEntropyLossDerivative<DeviceUsed, double>(deviceId);
                break;
            /*case UNSIGNED_INT:
                operatorBase = new SigmoidCrossEntropyLossDerivative<DeviceUsed, unsigned int>();

                break;*/
            case DataType::UNSIGNED_INT:
                return nullptr;
            }

            if (!setInput(operatorBase, "Input", tensors, deviceId) ||
                    !setInput(operatorBase, "Label", tensors, deviceId) ||
                    !setOutput(operatorBase, "Output", tensors, deviceId))
            {
                delete operatorBase;
                return nullptr;
            }


            return operatorBase;
        }

        template<DeviceType DeviceUsed>
        Operator<DeviceUsed> *initSoftmaxLogLoss(std::map<std::string, FreeWill::TensorDescriptor*> &tensors, int deviceId)
        {
            Operator<DeviceUsed> *operatorBase = nullptr;

            switch (m_dataType)
            {
            case DataType::FLOAT:
                operatorBase = new SoftmaxLogLoss<DeviceUsed, float>(deviceId);
                break;
            case DataType::DOUBLE:
                operatorBase = new SoftmaxLogLoss<DeviceUsed, double>(deviceId);
                break;
            /*case UNSIGNED_INT:
                operatorBase = new SoftmaxLogLoss<DeviceUsed, unsigned int>();
                break;*/
            case DataType::UNSIGNED_INT:
                return nullptr;
            }

            if (!setInput(operatorBase, "Input", tensors, deviceId) ||
                    !setInput(operatorBase, "Label", tensors, deviceId) ||
                    !setOutput(operatorBase, "Cost", tensors, deviceId) ||
                    !setOutput(operatorBase, "Output", tensors, deviceId))
            {
                delete operatorBase;
                return nullptr;
            }

            return operatorBase;
        }

        template<DeviceType DeviceUsed>
        Operator<DeviceUsed> *initSoftmaxLogLossDerivative(std::map<std::string, FreeWill::TensorDescriptor*> &tensors, int deviceId)
        {
            Operator<DeviceUsed> *operatorBase = nullptr;

            switch(m_dataType)
            {
            case DataType::FLOAT:
                operatorBase = new SoftmaxLogLossDerivative<DeviceUsed, float>(deviceId);
                break;
            case DataType::DOUBLE:
                operatorBase = new SoftmaxLogLossDerivative<DeviceUsed, double>(deviceId);
                break;
            /*case UNSIGNED_INT:
                operatorBase = new SoftmaxLogLossDerivative<DeviceUsed, unsigned int>();
                break;*/
            case DataType::UNSIGNED_INT:
                return nullptr;
            }

            if (!setInput(operatorBase, "Output", tensors, deviceId) ||
                    !setInput(operatorBase, "Label", tensors, deviceId) ||
                    !setOutput(operatorBase, "InputGrad", tensors, deviceId))
            {
                delete operatorBase;
                return nullptr;
            }

            return operatorBase;
        }

        template<DeviceType DeviceUsed>
        Operator<DeviceUsed> *initDuplicate(std::map<std::string, FreeWill::TensorDescriptor*> &tensors, int deviceId)
        {
            Operator<DeviceUsed> *operatorBase = nullptr;

            switch(m_dataType)
            {
            case DataType::FLOAT:
                operatorBase = new Duplicate<DeviceUsed, float>(deviceId);
                break;
            case DataType::DOUBLE:
                operatorBase = new Duplicate<DeviceUsed, double>(deviceId);
                break;
            case DataType::UNSIGNED_INT:
                operatorBase = new Duplicate<DeviceUsed, unsigned int>(deviceId);
                break;

            }

            if (!setInput(operatorBase, "From", tensors, deviceId) ||
                    !setOutput(operatorBase, "To", tensors, deviceId))
            {
                delete operatorBase;
                return nullptr;
            }

            return operatorBase;
        }

        template<DeviceType DeviceUsed>
        void reshape(std::map<std::string, FreeWill::TensorDescriptor*> &tensors, unsigned int deviceCount)
        {

            if (m_inputsNeedReshape.size() > 0)
            {
                for(unsigned int i = 0;i<m_inputsNeedReshape.size();++i)
                {
                    TensorDescriptor *tensorDescriptor = tensors[m_inputsNeedReshape[i].name()];
                    Shape newShape = m_inputsNeedReshape[i].shape();

                    for (unsigned int e = 0;e<deviceCount;++e)
                    {
                        TensorBase<DeviceUsed> *tensorBase = tensorDescriptor->getTensorForDevice<DeviceUsed>(e);

                        if (! tensorBase->reshape(tensorDescriptor->m_isBatchTensor?(newShape + tensorDescriptor->m_batchSize):newShape ))
                        {
                            std::cerr << "Reshape failed for tensor: " << tensorBase->name() << " from: " << tensorBase->shape() << " to: " << newShape << std::endl;
                            return;
                        }
                    }
                }
            }

            if (m_outputsNeedReshape.size() > 0)
            {
                for(unsigned int i = 0;i<m_outputsNeedReshape.size();++i)
                {
                    TensorDescriptor *tensorDescriptor = tensors[m_outputsNeedReshape[i].name()];
                    Shape newShape = m_outputsNeedReshape[i].shape();

                    for (unsigned int e = 0;e<deviceCount;++e)
                    {
                        TensorBase<DeviceUsed> *tensorBase = tensorDescriptor->getTensorForDevice<DeviceUsed>(e);

                        if (! tensorBase->reshape(tensorDescriptor->m_isBatchTensor?(newShape + tensorDescriptor->m_batchSize):newShape ))
                        {
                            std::cerr << "Reshape failed for tensor: " << tensorBase->name() << " from: " << tensorBase->shape() << " to: " << newShape << std::endl;
                            return;
                        }
                    }
                }
            }

        }

        template<DeviceType DeviceUsed>
        Operator<DeviceUsed> *initReshape(std::map<std::string, FreeWill::TensorDescriptor*> &tensors, int deviceId)
        {
            Operator<DeviceUsed> *operatorBase = nullptr;
            if (m_parameters.find("NewShape") == m_parameters.end())
            {
                qDebug() << "Reshape needs to specify newshape";
                return nullptr;
            }

            switch(m_dataType)
            {
            case DataType::FLOAT:
                operatorBase = new Reshape<DeviceUsed, float>(Shape(), deviceId);
                break;
            case DataType::DOUBLE:
                operatorBase = new Reshape<DeviceUsed, double>(Shape(), deviceId);
                break;
            case DataType::UNSIGNED_INT:
                operatorBase = new Reshape<DeviceUsed, unsigned int>(Shape(), deviceId);
                break;
            }

            if (!setInput(operatorBase, "Tensor", tensors, deviceId))
            {
                delete operatorBase;
                return nullptr;
            }
            return operatorBase;
        }

        template<DeviceType DeviceUsed>
        void evaluate(std::vector<WorkerMessage*> &messageQueue, std::map<std::string, FreeWill::TensorDescriptor*> &tensors)
        {
            int deviceId = 0;
            unsigned int deviceCount = m_operators[DeviceUsed].size();

            std::vector<WorkerMessage*> messages(deviceCount, nullptr);

            reshape<DeviceUsed>(tensors, deviceCount);

            auto iter = m_operators[DeviceUsed].begin();
            //auto startTime = std::chrono::steady_clock::now();

            for(;iter != m_operators[DeviceUsed].end(); ++iter)
            {
                Operator<DeviceUsed> *operatorBase = std::get<Operator<DeviceUsed>*>(*iter);
                messages[deviceId] = new WorkerMessage(WorkerMessage::Type::FORWARD, operatorBase);
                messages[deviceId]->debug_num = deviceId;
                Context<DeviceUsed>::getSingleton().pushWork(deviceId, messages[deviceId]);
                deviceId++;
                //messageQueue.push_back(message);
            }

            //auto middle = std::chrono::steady_clock::now();

            for(int i =0;i<deviceCount;++i)
            {
                messages[i]->join();
                delete messages[i];
            }

            //auto endTime = std::chrono::steady_clock::now();
            //auto diff = middle - startTime;

            //std::cout << "time for wait: " << std::chrono::duration <double, std::milli> (diff).count() << " ms used for 1 epoch" << std::endl;

        }

        template<DeviceType DeviceUsed>
        void evaluateWithParameterUpdate(const std::map<std::string, std::any> &newParameters, std::map<std::string, FreeWill::TensorDescriptor*> &tensors)
        {
            auto iter = m_operators[DeviceUsed].begin();

            int deviceId = 0;
            unsigned int deviceCount = m_operators[DeviceUsed].size();

            std::vector<WorkerMessage*> messages(deviceCount, nullptr);

            reshape<DeviceUsed>(tensors, deviceCount);


            for(;iter != m_operators[DeviceUsed].end(); ++iter)
            {
                Operator<DeviceUsed> *operatorBase = std::get<Operator<DeviceUsed>*>(*iter);
                switch(m_operatorName)
                {
                case FreeWill::OperatorName::ACTIVATION:
                case FreeWill::OperatorName::ACTIVATION_DERIVATIVE:
                case FreeWill::OperatorName::CONVOLUTION:
                case FreeWill::OperatorName::CONVOLUTION_DERIVATIVE:
                case FreeWill::OperatorName::CROSS_ENTROPY_LOSS:
                case FreeWill::OperatorName::DOT_PRODUCT_WITH_BIAS:
                case FreeWill::OperatorName::DOT_PRODUCT_WITH_BIAS_DERIVATIVE:
                case FreeWill::OperatorName::MAX_POOLING:
                case FreeWill::OperatorName::MAX_POOLING_DERIVATIVE:
                case FreeWill::OperatorName::SIGMOID_CROSS_ENTROPY_LOSS_DERIVATIVE:
                case FreeWill::OperatorName::SOFTMAX_LOG_LOSS:
                case FreeWill::OperatorName::SOFTMAX_LOG_LOSS_DERIVATIVE:
                case FreeWill::OperatorName::RESHAPE:
                    break;
                case FreeWill::OperatorName::ELEMENTWISE_ADD:
                    if (newParameters.find("Rate") != newParameters.end())
                    {
                        float rate = std::any_cast<float>(newParameters.at("Rate"));
                        switch(m_dataType)
                        {
                        case DataType::FLOAT:
                            dynamic_cast<ElementwiseAdd<DeviceUsed, float>*>(operatorBase)->setRate(rate);
                            break;
                        case DataType::DOUBLE:
                            dynamic_cast<ElementwiseAdd<DeviceUsed, double>*>(operatorBase)->setRate(rate);
                            break;
                        case DataType::UNSIGNED_INT:
                            break;
                        }
                    }
                    break;
                }

                //operatorBase->evaluate();

                messages[deviceId] /*WorkerMessage *message*/ = new WorkerMessage(WorkerMessage::Type::FORWARD, operatorBase);
                Context<DeviceUsed>::getSingleton().pushWork(deviceId, messages[deviceId] /* message*/);
                deviceId++;
            }

            for(int i =0;i<deviceCount;++i)
            {
                messages[i]->join();
                delete messages[i];
            }

        }

        template<DeviceType DeviceUsed = DeviceType::CPU_NAIVE>
        bool init(std::map<std::string, TensorDescriptor*> &tensors)
        {
            int deviceCount = Context<DeviceUsed>::getSingleton().deviceCount();

            qDebug() << "init operator" << m_name.c_str();

            for(int i =0;i<deviceCount;++i)
            {
                Operator<DeviceUsed> *operatorBase = nullptr;

                switch(m_operatorName)
                {
                case OperatorName::ACTIVATION:
                    operatorBase = initActivation<DeviceUsed>(tensors, i);
                break;
                case OperatorName::ACTIVATION_DERIVATIVE:
                    operatorBase = initActivationDerivative<DeviceUsed>(tensors, i);
                break;
                case OperatorName::CONVOLUTION:
                    operatorBase = initConvolution<DeviceUsed>(tensors, i);
                break;
                case OperatorName::CONVOLUTION_DERIVATIVE:
                    operatorBase = initConvolutionDerivative<DeviceUsed>(tensors, i);
                break;
                case OperatorName::CROSS_ENTROPY_LOSS:
                    operatorBase = initCrossEntropyLoss<DeviceUsed>(tensors, i);
                break;
                case OperatorName::DOT_PRODUCT_WITH_BIAS:
                    operatorBase = initDotProductWithBias<DeviceUsed>(tensors, i);
                break;
                case OperatorName::DOT_PRODUCT_WITH_BIAS_DERIVATIVE:
                    operatorBase = initDotProductWithBiasDerivative<DeviceUsed>(tensors, i);
                break;
                case OperatorName::ELEMENTWISE_ADD:
                    operatorBase = initElementwiseAdd<DeviceUsed>(tensors, i);
                break;
                case OperatorName::MAX_POOLING:
                    operatorBase = initMaxPooling<DeviceUsed>(tensors, i);
                break;
                case OperatorName::MAX_POOLING_DERIVATIVE:
                    operatorBase = initMaxPoolingDerivative<DeviceUsed>(tensors, i);
                break;
                case OperatorName::SIGMOID_CROSS_ENTROPY_LOSS_DERIVATIVE:
                    operatorBase = initSigmoidCrossEntropyLossDerivative<DeviceUsed>(tensors, i);
                break;
                case OperatorName::SOFTMAX_LOG_LOSS:
                    operatorBase = initSoftmaxLogLoss<DeviceUsed>(tensors, i);
                break;
                case OperatorName::SOFTMAX_LOG_LOSS_DERIVATIVE:
                    operatorBase = initSoftmaxLogLossDerivative<DeviceUsed>(tensors, i);
                break;
                case OperatorName::DUPLICATE:
                    operatorBase = initDuplicate<DeviceUsed>(tensors, i);
                break;
                case OperatorName::RESHAPE:
                    operatorBase = initReshape<DeviceUsed>(tensors, i);
                break;
                }

                if (!operatorBase)
                {
                    return false;
                }

                reshape<DeviceUsed>(tensors, deviceCount);

                if (!operatorBase->init())
                {
                    delete operatorBase;
                    return false;
                }

                m_operators[DeviceUsed].push_back(operatorBase);
            }
            return true;
        }
    };
}

#endif
