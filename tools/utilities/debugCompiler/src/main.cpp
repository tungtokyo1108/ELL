////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Project:  Embedded Learning Library (ELL)
//  File:     main.cpp (debugCompiler)
//  Authors:  Chris Lovett
//
////////////////////////////////////////////////////////////////////////////////////////////////////

#include "CompareArguments.h"
#include "ModelComparison.h"
#include "ReplaceSourceAndSinkNodesPass.h"

#include <pythonPlugins/include/InvokePython.h>

#include <common/include/LoadModel.h>
#include <common/include/MapCompilerArguments.h>
#include <common/include/ModelLoadArguments.h>

#include <model/include/Map.h>
#include <model/include/MapCompilerOptions.h>
#include <model/include/PortMemoryLayout.h>

#include <utilities/include/CommandLineParser.h>
#include <utilities/include/Exception.h>
#include <utilities/include/Files.h>
#include <utilities/include/OutputStreamImpostor.h>
#include <utilities/include/RandomEngines.h>
#include <utilities/include/TypeTraits.h>

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using namespace ell;

template <typename InputType, utilities::IsIntegral<InputType> = true>
std::vector<InputType> GetInputVector(const model::MemoryShape& inputShape)
{
    auto inputSize = inputShape.NumElements();
    std::vector<InputType> result(inputSize);
    auto engine = utilities::GetRandomEngine("123");
    std::uniform_int_distribution<InputType> dist(0, 255);
    for (auto index = 0; index < inputSize; ++index)
    {
        result[index] = dist(engine);
    }
    return result;
}

template <typename InputType, utilities::IsFloatingPoint<InputType> = true>
std::vector<InputType> GetInputVector(const model::MemoryShape& inputShape)
{
    auto inputSize = inputShape.NumElements();
    std::vector<InputType> result(inputSize);
    auto engine = utilities::GetRandomEngine("123");
    std::uniform_real_distribution<InputType> dist(0, std::nextafter(255, std::numeric_limits<InputType>::max()));
    for (int index = 0; index < inputSize; ++index)
    {
        result[index] = dist(engine);
    }
    return result;
}

void ReplaceSourceAndSinkNodes(model::Map& map)
{
    model::MapCompilerOptions settings;
    model::ModelOptimizer optimizer(settings);
    optimizer.AddPass(std::make_unique<ReplaceSourceAndSinkNodesPass>());
    map.RemoveInputs();
    map.Optimize(optimizer);

    // now put back inputs
    auto inputNodes = map.GetModel().GetNodesByType<model::InputNodeBase>();
    int index = 1;
    for (auto node : inputNodes)
    {
        map.AddInput("input_" + std::to_string(index), node);
        ++index;
    }
}

template <typename InputType>
std::vector<InputType> GetInputConverted(std::string filename, const std::vector<std::string>& args)
{
    auto result = ExecutePythonScript(filename, args);
    return std::vector<InputType>(result.begin(), result.end());
}

template <typename InputType>
std::vector<InputType> GetInputData(model::Map& map, const CompareArguments& compareArguments, const std::vector<std::string>& args)
{
    auto inputShape = map.GetInputShape();
    if (compareArguments.inputConverter.empty())
    {
        return GetInputVector<InputType>(inputShape);
    }
    else
    {
        return GetInputConverted<InputType>(compareArguments.inputConverter, args);
    }
}

int main(int argc, char* argv[])
{
    using TestDataType = float;
    try
    {
        // create a command line parser
        utilities::CommandLineParser commandLineParser(argc, argv);

        // add arguments to the command line parser
        ParsedCompareArguments compareArguments;
        commandLineParser.AddOptionSet(compareArguments);
        common::ParsedMapCompilerArguments compileArguments;
        commandLineParser.AddDocumentationString("Code generation options");
        commandLineParser.AddOptionSet(compileArguments);
        commandLineParser.Parse();

        if (compareArguments.inputMapFile.empty())
        {
            std::cout << "Model file not specified\n\n";
            std::cout << commandLineParser.GetHelpString() << std::endl;
            return 1;
        }

        if (!ell::utilities::FileExists(compareArguments.inputMapFile))
        {

            std::cout << "Model file not found: " << compareArguments.inputMapFile << std::endl;
            std::cout << commandLineParser.GetHelpString() << std::endl;
            return 1;
        }

        // load map file
        std::cout << "loading map..." << std::endl;
        model::Map map = common::LoadMap(compareArguments.inputMapFile);

        ReplaceSourceAndSinkNodes(map);

        if (compareArguments.outputDirectory != "")
        {
            ell::utilities::EnsureDirectoryExists(compareArguments.outputDirectory);
        }

        auto pluginArgs = commandLineParser.GetPassthroughArgs();
        auto input = GetInputData<TestDataType>(map, compareArguments, pluginArgs);
        ModelComparison comparison(compareArguments.outputDirectory);

        model::MapCompilerOptions settings = compileArguments.GetMapCompilerOptions("");
        comparison.Compare(input, map, settings);

        // Write summary report
        if (compareArguments.writeReport)
        {
            std::string reportFileName = utilities::JoinPaths(compareArguments.outputDirectory, "report.md");
            std::ofstream reportStream(reportFileName);
            comparison.WriteReport(reportStream, compareArguments.inputMapFile, pluginArgs, compareArguments.writePrediction);
        }

        // Write an annotated graph showing where differences occurred in the model
        // between compiled and reference implementation.
        if (compareArguments.writeGraph)
        {
            std::string graphFileName = utilities::JoinPaths(compareArguments.outputDirectory, "graph.dgml");
            std::ofstream graphStream(graphFileName);
            comparison.SaveDgml(graphStream);

            std::string dotFileName = utilities::JoinPaths(compareArguments.outputDirectory, "graph.dot");
            std::ofstream dotStream(dotFileName);
            comparison.SaveDot(dotStream);
        }
    }
    catch (const utilities::CommandLineParserPrintHelpException& exception)
    {
        std::cout << exception.GetHelpText() << std::endl;
        return 0;
    }
    catch (const utilities::CommandLineParserErrorException& exception)
    {
        std::cerr << "Command line parse error:" << std::endl;
        for (const auto& error : exception.GetParseErrors())
        {
            std::cerr << error.GetMessage() << std::endl;
        }
        return 1;
    }
    catch (const utilities::Exception& exception)
    {
        std::cerr << "runtime error: " << exception.GetMessage() << std::endl;
        return 1;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "runtime error: " << ex.what() << std::endl;
        return 1;
    }

    // the end
    return 0;
}
