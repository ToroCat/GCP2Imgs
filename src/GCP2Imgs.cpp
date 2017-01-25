/////////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2015, Toro Lee. Use, modification and
// distribution are subject to the CeCILL-B License
// Author(s): Toro Lee <poy49295@163.com>
// For more informations, see the link below.
// http://forum-micmac.forumprod.com/question-about-saisieappuispredicqt-t1023.html
//
/////////////////////////////////////////////////////////////////////////////////////

// C++ Standard Libraries
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <set>
#include <sstream>
#include <algorithm>
#include <map>
#include <functional>
#include <fstream>

// External dependences(Only Boost)
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/system/error_code.hpp>
#include <boost/regex.hpp>

// RapidXml is a head only library
// so I just import the whole file here
#include "rapidxml.hpp"
#include "ProcessInvoke.h"

// using declaration
// to avoid name space pollution
using std::cout;
using std::endl;
using std::string;
using std::vector;
using std::set;
using std::ostringstream;
using std::map;
using std::function;
using std::remove;
using std::transform;
using std::getline;
using std::ifstream;
using std::mismatch;

using boost::filesystem::path;
using boost::filesystem::is_directory;
using boost::filesystem::is_regular_file;
using boost::filesystem::directory_iterator;
using boost::filesystem::initial_path;
using boost::system::error_code;

using rapidxml::xml_document;
using rapidxml::xml_node;


// Unnamed name space can avoid naming conflicts in multi compile units
namespace
{
// some global constants

// execution arguments
constexpr int g_mandatoryArgCount = 3;
constexpr size_t g_allImg = 1;
constexpr size_t g_oriArgIndex = 2;
constexpr size_t g_gcpFileArgIndex = 3;

const char* const g_coordFileName = "GCP-Coordinates.txt";
const char* const g_oriDirPrefix = "Ori-";

// effect: SomeName -> Ori-SomeName
void AddOriPrefixIfNotExisted(string *oriDirName)
{
    if(0 == memcmp(g_oriDirPrefix, oriDirName->data(), strlen(g_oriDirPrefix)))
    {
        // alread added
        return;
    }
    oriDirName->insert(0, g_oriDirPrefix);
}

// print the help in command line
void PrintHelp()
{
    cout<<endl<<
"*****************************\n\
*  Help for Elise Arg main  *\n\
*****************************\n\
Mandatory unnamed args :\n\
  * string :: {Full Directory (Dir+Pattern)}\n\
  * string :: {Orientations}\n\
  * string :: {Ground Control Points File}\n\
Named args :\n\
  * [Name=Out] string :: {Directory of Output Fils(s), Default=GCP-IMG}\n\
  * [Name=Pattern] bool :: {Output in pattern or images list, Default=true}\n\
  * [Name=InitPath] string :: {mm3d bin path}\n"<<endl;
}

bool ValidateArgumentsAndPrompt(const path &oriDirPath, const path &gcpFilePath)
{
    // validate orientations directory
    if(false == is_directory(oriDirPath))
    {
        cout<<endl<<"Cannot find orientations directory: "<<oriDirPath.string()<<endl;
        return false;
    }
    // validate Ground Control Points File
    if(false == is_regular_file(gcpFilePath))
    {
        cout<<endl<<"Cannot find Ground Control Points File: "<<gcpFilePath.string()<<endl;
        return false;
    }
    // the third argument may be a GCP name or list file path
    // postpone its validaion later
    return true;
}

// filter the images that user excludes by regular expression
bool FileterImagesByPattern(const string &allImagePattern, set<string> *imagesList)
{
    imagesList->clear();
    boost::regex regularExpression;
    const path patternPath(allImagePattern);
    regularExpression.set_expression(patternPath.filename().string());
    boost::match_results<string::const_iterator> what;
    string imageName;
    for(directory_iterator iter(patternPath.parent_path()), end; end != iter; ++iter)
    {
        if(false == is_regular_file(*iter))
        {
            // we only focus on image file
            continue;
        }
        imageName = iter->path().filename().string();
        if(false == boost::regex_match(imageName, what, regularExpression))
        {
            continue;
        }
        imagesList->insert(imageName);
    }
    return false == imagesList->empty();
}

// read GCP XML file content
bool ReadGcpXmlFile(const char* const gcpFilePath, xml_document<> *gcpXml)
{
    char readBuffer[2048];
    string xmlContent;
    FILE *gcpFileHandle = fopen(gcpFilePath, "rb");
    if(nullptr == gcpFileHandle)
    {
        cout<<endl<<"Cannot open GCP file: "<<gcpFilePath<<endl;
        return false;
    }
    while(true)
    {
        const size_t byteRead = fread(readBuffer, 1, sizeof(readBuffer), gcpFileHandle);
        if(0 == byteRead)
        {
            break;
        }
        xmlContent.append(readBuffer, readBuffer+byteRead);
    }
    fclose(gcpFileHandle);
    gcpXml->parse<rapidxml::parse_no_utf8>((char*)xmlContent.c_str());
    return true;
}

// GCP data structure
struct GcpData
{
    string name;
    double x, y, z;
};

// fetch all GCP data from XML file and fill into gcpDat
bool FetchAllGcps(const char* const gcpFilePath, vector<GcpData> *gcpDat)
{
    gcpDat->clear();
    xml_document<> gcpXml;
    if(false == ReadGcpXmlFile(gcpFilePath, &gcpXml))
    {
        return false;
    }
    const xml_node<>* const dicoAppuisFlottant = gcpXml.first_node("DicoAppuisFlottant");
    if(nullptr == dicoAppuisFlottant)
    {
        cout<<endl<<"Invalid GCP file, target node name: dicoAppuisFlottant"<<endl;
        return false;
    }
    string tmp;
    for(const xml_node<> *gcpNode = dicoAppuisFlottant->first_node("OneAppuisDAF");
        nullptr != gcpNode; gcpNode = gcpNode->next_sibling())
    {
        const xml_node<> *coord = gcpNode->first_node("Pt");
        if(nullptr == coord)
        {
            cout<<endl<<"Invalid GCP file, cannot find coordinate field"<<endl;
            return false;
        }
        const xml_node<> *gcpName = gcpNode->first_node("NamePt");
        if(nullptr == gcpName)
        {
            cout<<endl<<"Invalid GCP file, cannot find GCP name field"<<endl;
            return false;
        }
        tmp.assign(coord->value(), coord->value()+coord->value_size());
        const size_t firstSpace = tmp.find_first_of(' ');
        if(string::npos == firstSpace)
        {
            cout<<endl<<"Invalid GCP file: cannot find first space"<<endl;
            return false;
        }
        const size_t secondSpace = tmp.find_first_of(' ', firstSpace+1);
        if(string::npos == secondSpace)
        {
            cout<<endl<<"Invalid GCP file: cannot second first space"<<endl;
            return false;
        }
        gcpDat->emplace_back();
        GcpData &dat = gcpDat->back();
        dat.x = atof(tmp.substr(0, firstSpace).c_str());
        dat.y = atof(tmp.substr(firstSpace+1, secondSpace-firstSpace-1).c_str());
        dat.z = atof(tmp.substr(secondSpace+1).c_str());
        dat.name.assign(gcpName->value(), gcpName->value()+gcpName->value_size());
    }
    return true;
}

// create coordinates file for XYZ2Im
bool WriteGcpsCoordToFile(const path &outputDir, const vector<GcpData> &gcpDat)
{
    ostringstream stringStream;
    stringStream<<std::setprecision(3)<<std::fixed;
    for(const auto &gcp : gcpDat)
    {
        stringStream<<gcp.x<<' '<<gcp.y<<' '<<gcp.z<<endl;
    }
    string posContent(stringStream.str());
    // remove the last endl
    posContent.pop_back();

    FILE *fileHandle = fopen((outputDir/g_coordFileName).string().c_str(), "wb");
    if(nullptr == fileHandle)
    {
        cout<<endl<<"Cannot create GCP coordinates to file"<<endl;
        return false;
    }
    fwrite(posContent.data(), 1, posContent.size(), fileHandle);
    fclose(fileHandle);
    return true;
}

// parse and fetch optional argument
void FetchOptionalArg(const int argc,char **argv,
                      string *outputDirName, string *initPath, bool *pattern)
{
    if(g_mandatoryArgCount+1 >= argc)
    {
        return;
    }
    map<string,function<void(const string&)>> funcMap;
    funcMap["Out"] = [outputDirName](const string &value){*outputDirName = value;};
    funcMap["Pattern"] = [pattern](const string &value)
    {
        string tmp(value);
        transform(tmp.begin(), tmp.end(), tmp.begin(), ::tolower);
        if(tmp == "true")
        {
            *pattern = true;
        }
        else if(tmp == "false" || 0==atoi(tmp.c_str()))
        {
            *pattern = false;
        }
    };
    funcMap["InitPath"] = [initPath](const string &value){*initPath = value;};
    string argument;
    for(int index = g_mandatoryArgCount+1;argc != index; ++index)
    {
        argument = argv[index];
        const size_t equaIndex = argument.find_first_of('=');
        if(string::npos == equaIndex)
        {
            continue;
        }
        auto mapIter = funcMap.find(argument.substr(0, equaIndex));
        if(mapIter == funcMap.end())
        {
            continue;
        }
        mapIter->second(argument.substr(equaIndex+1));
    }
}

// effect: SomeName.txt -> SomeName-postfix.txt
void AddPostfix(const char *postfix, string *fileName)
{
    size_t dotIndex = fileName->find_last_of('.');
    if(string::npos == dotIndex)
    {
        dotIndex = fileName->size();
    }
    fileName->insert(dotIndex, postfix);
}

// create images pattern in regular expression from images list(imagesName)
void GetImagesPattern(const vector<string> &imagesName, string *pattern)
{
    const size_t count = imagesName.size();
    pattern->clear();
    if(0 == count)
    {
        return;
    }
    else if(1 == count)
    {
        *pattern = imagesName[0];
        return;
    }
    const string &firstString = imagesName.front();
    assert(false == firstString.empty() && "encounter empty string");
    const long firstStringSize = firstString.size();
    long firstDiff = firstStringSize, lastDiffR = firstStringSize;

    const auto firstBegin = firstString.cbegin(), firstEnd = firstString.cend();
    const auto firstRbegin = firstString.crbegin(), firstRend = firstString.crend();
    for(size_t index = 1;count != index; ++index)
    {
        const string &curIter = imagesName[index];
        assert(false==curIter.empty() && "encounter empty string");
        const auto matchPairFromLeft = mismatch(firstBegin, firstEnd, curIter.cbegin());
        const long thisFirstDiff = matchPairFromLeft.first - firstBegin;
        if(firstDiff > thisFirstDiff)
        {
            firstDiff = thisFirstDiff;
        }
        const auto matchPairFromRight = mismatch(firstRbegin, firstRend, curIter.crbegin());
        const long thisLastDiffR = matchPairFromRight.first - firstRbegin;
        if(lastDiffR > thisLastDiffR)
        {
            lastDiffR = thisLastDiffR;
        }
    }
    pattern->assign(firstBegin, firstBegin+firstDiff);
    pattern->push_back('(');
    for(const auto &name : imagesName)
    {
        pattern->append(name.cbegin()+firstDiff, name.cend()-lastDiffR);
        pattern->push_back('|');
    }
    pattern->back() = ')';
    pattern->append(firstBegin+(firstStringSize-lastDiffR), firstEnd);
}

using Gcp2ImgsMapType = map<string,vector<string>>;

// write the final result to file
bool WriteGcp2ImgsToFile(const Gcp2ImgsMapType &gcp2ImgsMap, const path &outputDir, bool pattern)
{
    string fileContent;
    string outputFilePath;
    if(false == is_directory(outputDir))
    {
        error_code errorCode;
        create_directory(outputDir, errorCode);
        if(errorCode)
        {
            cout<<"Cannot create directory: "<<outputDir.string()<<endl;
            return false;
        }
    }
    assert(false == gcp2ImgsMap.empty() && "\ngcp2ImgsMap Cannot be empty\n");
    for(const auto &record : gcp2ImgsMap)
    {
        fileContent.clear();
        if(pattern)
        {
            GetImagesPattern(record.second, &fileContent);
        }
        else
        {
            for(const auto &img : record.second)
            {
                fileContent += img;
                fileContent.push_back('\n');
            }
            fileContent.pop_back();
        }
        outputFilePath = (outputDir/(record.first+"-GCP2IMGS.txt")).string();
        FILE *fileHandle = fopen(outputFilePath.c_str(), "wb") ;
        if(nullptr == fileHandle)
        {
            cout<<"Cannot create file: "<<outputFilePath<<endl;
            continue;
        }
        fwrite(fileContent.data(), 1, fileContent.size(), fileHandle);
        fclose(fileHandle);
    }
    return true;
}

/// EXIF simple structure, only contain the fields I interest
struct Exif
{
    std::string name;
    //size_t fileSize;
    //std::string mimeType;
    size_t width;
    size_t height;
    // [TODO] to be continue
};

void ExtractImageName(string &text, Exif *exif)
{
    exif->name = path(text).filename().string();
    // remove character '\n'
    const auto iter = remove(exif->name.begin(), exif->name.end(), '\n');
    exif->name.erase(iter, exif->name.end());
}

void ExtractImageSize(string &text, Exif *exif)
{
    const auto removedEnd = remove(text.begin(), text.end(), ' ');
    const size_t xPos = text.find_first_of('x');
    if(string::npos == xPos)
    {
        return;
    }
    const auto begin = text.begin();
    exif->width = atoi(string(begin, begin+xPos).c_str());
    exif->height = atoi(string(begin+xPos+1, removedEnd).c_str());
}

bool GetImageFileExif(const string &imageFilePath, const string &exivBinPath, Exif *exif)
{
    if(false == is_regular_file(path(imageFilePath)))
    {
        return false;
    }
    const vector<string> arguments = {"pr", imageFilePath};
    ProcessInvoke("", exivBinPath, arguments, [exif](const char *text)
    {
        map<string,function<void(string&, Exif*)>> infoExtactorMap;
        infoExtactorMap[string("imagesize")] = ExtractImageSize;
        infoExtactorMap[string("filename")] = ExtractImageName;
        // set ':' as a seperator
        const char *colon = strchr(text, ':');
        if(nullptr == colon)
        {
            return;
        }
        string firstPart(text, colon);
        // remove all white space
        const auto removedEnd = remove(firstPart.begin(), firstPart.end(), ' ');
        firstPart.resize(removedEnd - firstPart.begin());
        transform(firstPart.begin(), firstPart.end(), firstPart.begin(), ::tolower);
        const auto extractor = infoExtactorMap.find(firstPart);
        if(extractor == infoExtactorMap.end())
        {
            return;
        }
        string secondPart(colon+1, text+strlen(text));
        extractor->second(secondPart, exif);
    });
    return true;
}

// update the mapping data, the map is about GCP to images
void UpdateGcp2ImgsMap(const vector<GcpData> &gcpDat,
                       const string &gcpInImgCoordsFilePath,
                       const Exif &exif, Gcp2ImgsMapType *targetMap)
{
    ifstream inFile;
    inFile.open(gcpInImgCoordsFilePath, std::ifstream::binary|std::ifstream::in);
    if(false == inFile.is_open())
    {
        cout<<"not found image coordinate file: "<<gcpInImgCoordsFilePath<<endl;
        return;
    }
    string coordText;
    for(size_t gcpIndex = 0; inFile.good(); ++gcpIndex)
    {
        getline(inFile, coordText);
        const size_t spaceIndex = coordText.find_first_of(' ');
        if(string::npos == spaceIndex)
        {
            continue;
        }
        const double x = atof(coordText.substr(0, spaceIndex).c_str());
        if(x < 0.0 || x > exif.width)
        {
            continue;
        }
        const double y = atof(coordText.substr(spaceIndex+1).c_str());
        if(y < 0.0 || y > exif.height)
        {
            continue;
        }
        (*targetMap)[gcpDat[gcpIndex].name].push_back(exif.name);
    }
    inFile.close();
}



bool MakeGcpToImagesMappingFile(const string &initPath,
                                const path &datasetRoot, const path &oriDirPath,
                                const set<string> &selectedImages,
                                const vector<GcpData> &gcpDat,
                                const string &coordFilePath,
                                const path &outputDir, bool pattern)
{
    assert((false == gcpDat.empty()) && (false == selectedImages.empty()) && "No GCP data");

#if BOOST_OS_WINDOWS != 0
    const string exivBinPath((path(initPath).parent_path()/"binaire-aux/windows/exiv2.exe").string());
#elif (BOOST_OS_LINUX!=0) || (BOOST_OS_MACOS!=0)
	// assume the system already install exiv2    
    const string exivBinPath("exiv2.exe").string());
#endif
    if(false == is_regular_file(exivBinPath))
    {
        cout<<"Cannot find exiv2: "<<exivBinPath<<endl;
        return false;
    }
    // mm3d XYZ2Im "Ori-GcpInitOri/Orientation-DSC_6443.jpg.xml" coordinates.txt DSC_6443-GCP.txt
    vector<string> arguments = {"XYZ2Im", "", coordFilePath, ""};
    const auto callback = [](const char *text){cout<<text;};
    Gcp2ImgsMapType gcp2ImgsMap;
    error_code errorCode;
    Exif exif;

    for(const auto &imageFileName : selectedImages)
    {
        string &oriFilePath = arguments[1];
        oriFilePath = "Orientation-";
        oriFilePath.append(imageFileName);
        oriFilePath.append(".xml");
        oriFilePath = (oriDirPath/oriFilePath).string();

        string &imgCoordFileName = arguments[3];
        imgCoordFileName = imageFileName;
        AddPostfix("-GCP", &imgCoordFileName);
        imgCoordFileName.append(".txt");
        imgCoordFileName = (datasetRoot/imgCoordFileName).string();
        ProcessInvoke("", "mm3d", arguments, callback);
        if(false == GetImageFileExif((datasetRoot/imageFileName).string(),
                                     exivBinPath, &exif))
        {
            cout<<"Error in getting image EXIF: "<<(datasetRoot/imageFileName).string()<<endl;
            continue;
        }
        UpdateGcp2ImgsMap(gcpDat, imgCoordFileName, exif, &gcp2ImgsMap);
        remove(path(imgCoordFileName), errorCode);
    }
    remove(path(coordFilePath), errorCode);
    // write result
    return WriteGcp2ImgsToFile(gcp2ImgsMap, outputDir, pattern);
}

}

int main(int argc,char **argv)
{
    if(g_mandatoryArgCount+1 > argc)
    {
        // too few argument(s), print the help then exit
        PrintHelp();
        return 1;
    }
    const string allImagePattern(argv[g_allImg]);
    const path datasetRoot = path(allImagePattern).parent_path();
    string oriDirName(argv[g_oriArgIndex]);
    AddOriPrefixIfNotExisted(&oriDirName);
    const path oriDirPath(datasetRoot/oriDirName);
    const path gcpFilePath(datasetRoot/argv[g_gcpFileArgIndex]);
    if(false == ValidateArgumentsAndPrompt(oriDirPath, gcpFilePath))
    {
        // something goes wrong
        return 1;
    }
    set<string> selectedImages;
    if(false == FileterImagesByPattern(allImagePattern, &selectedImages))
    {
        // something goes wrong
        return 1;
    }
    vector<GcpData> gcpDat;
    if(false == FetchAllGcps(gcpFilePath.string().c_str(), &gcpDat))
    {
        // something goes wrong
        return 1;
    }
    if(false == WriteGcpsCoordToFile(datasetRoot, gcpDat))
    {
        // something goes wrong
        return 1;
    }
    // default setting
    string outputDirName("GCP-IMG"), initPath(initial_path().string());
    bool pattern = true;
    FetchOptionalArg(argc, argv, &outputDirName, &initPath, &pattern);
    const string coordFilePath((datasetRoot/g_coordFileName).string());
    return MakeGcpToImagesMappingFile(initPath, datasetRoot, oriDirPath,
                                      selectedImages, gcpDat, coordFilePath,
                                      datasetRoot/outputDirName, pattern) ? 0 : 1;
}
