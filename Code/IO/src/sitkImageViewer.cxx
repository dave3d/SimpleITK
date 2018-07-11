/*=========================================================================
*
*  Copyright Insight Software Consortium
*
*  Licensed under the Apache License, Version 2.0 (the "License");
*  you may not use this file except in compliance with the License.
*  You may obtain a copy of the License at
*
*         http://www.apache.org/licenses/LICENSE-2.0.txt
*
*  Unless required by applicable law or agreed to in writing, software
*  distributed under the License is distributed on an "AS IS" BASIS,
*  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*  See the License for the specific language governing permissions and
*  limitations under the License.
*
*=========================================================================*/
#include "sitkImageViewer.h"
#include "sitkMacro.h"
#include "sitkImageFileWriter.h"
#include <itkMacro.h>
#include <itksys/SystemTools.hxx>
#include <itksys/Process.h>
#include <stdlib.h>
#include <iostream>
#include <sstream>
#include <iterator>
#include <string>
#include <algorithm>
#include <ctype.h>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

#define localDebugMacro(x)\
  {                                                                     \
    if (ImageViewer::DebugOn)                                                        \
      {                                                                 \
      std::ostringstream msg;                                           \
      msg << "Debug: In " __FILE__ ", line " << __LINE__ << ": " x      \
          << "\n\n";                                                    \
      ::itk::OutputWindowDisplayDebugText( msg.str().c_str() );         \
      }                                                                 \
  }                                                                     \

#define IMAGEJ_OPEN_MACRO "open(\"%f\"); rename(\"%t\");"
#define NIFTI_COLOR_MACRO " run(\"Make Composite\", \"display=Composite\");"

namespace itk
{
  namespace simple
  {

int ImageViewer::ViewerImageCount=0;
bool ImageViewer::AreDefaultsInitialized=false;
bool ImageViewer::DebugOn=true;

std::vector<std::string> ImageViewer::SearchPath;
std::vector<std::string> ImageViewer::ExecutableNames;

std::string ImageViewer::DefaultViewCommand;
std::string ImageViewer::DefaultViewColorCommand;
std::string ImageViewer::DefaultFijiCommand;

std::string ImageViewer::DefaultApplication;
std::string ImageViewer::DefaultFileExtension;

void ImageViewer::initializeDefaults()
  {
  if (AreDefaultsInitialized)
    return;

  std::string Extension;
  std::string cmd;

  // check environment variables for user specified strings

  // File Extension
  itksys::SystemTools::GetEnv ( "SITK_SHOW_EXTENSION", Extension );
  if (Extension.length()>0)
    {
    DefaultFileExtension = Extension;
    }
  else
    {
    DefaultFileExtension = ".nii";
    }

  // Show command
  itksys::SystemTools::GetEnv ( "SITK_SHOW_COMMAND", cmd );
  if (cmd.length()>0)
    {
    DefaultViewCommand = cmd;
    }
  else
    {
#if defined(_WIN32)
    DefaultViewCommand = "%a -eval \'" IMAGEJ_OPEN_MACRO "\'";
#elif defined(__APPLE__)
    DefaultViewCommand = "open -a %a -n --args -eval \'" IMAGEJ_OPEN_MACRO "\'";
#else
    // Linux
    DefaultViewCommand = "%a -e \'" IMAGEJ_OPEN_MACRO "\'";
#endif
    }

  // Show Color Command
  itksys::SystemTools::GetEnv ( "SITK_SHOW_COLOR_COMMAND", cmd );
  if (cmd.length()>0)
    {
    DefaultViewColorCommand = cmd;
    }
  else
    {
#if defined(_WIN32)
    DefaultViewColorCommand = "%a -eval \'" IMAGEJ_OPEN_MACRO NIFTI_COLOR_MACRO "\'";
#elif defined(__APPLE__)
    DefaultViewCommand = "open -a %a -n --args -eval \'" IMAGEJ_OPEN_MACRO NIFTI_COLOR_MACRO "\'";
#else
    // Linux
    DefaultViewCommand = "%a -e \'" IMAGEJ_OPEN_MACRO NIFTI_COLOR_MACRO "\'";
#endif
    }

  // Fiji Command
  //
  // For Fiji, we only need 2 commands, not 6.  We don't need a separate command for color images.
  // Also the linux version uses the "-eval" flag instead of "-e".
#if defined(__APPLE__)
  DefaultFijiCommand = "open -a %a -n --args -eval \'" IMAGEJ_OPEN_MACRO "\'";
#else
  // Linux & Windows
  DefaultFijiCommand = "%a -eval \'" IMAGEJ_OPEN_MACRO "\'";
#endif


  //
  // Build the SearchPath
  //
#ifdef _WIN32

  std::string ProgramFiles;
  if ( itksys::SystemTools::GetEnv ( "PROGRAMFILES", ProgramFiles ) )
    {
    SearchPath.push_back ( ProgramFiles + "\\" );
    }

  if ( itksys::SystemTools::GetEnv ( "PROGRAMFILES(x86)", ProgramFiles ) )
    {
    SearchPath.push_back ( ProgramFiles + "\\" );
    }

  if ( itksys::SystemTools::GetEnv ( "PROGRAMW6432", ProgramFiles ) )
    {
    SearchPath.push_back ( ProgramFiles + "\\" );
    }

  if ( itksys::SystemTools::GetEnv ( "USERPROFILE", ProgramFiles ) )
    {
    SearchPath.push_back ( ProgramFiles + "\\" );
    SearchPath.push_back ( ProgramFiles + "\\Desktop\\" );
    }

#elif defined(__APPLE__)

  // Common places on the Mac to look
  SearchPath.push_back( "/Applications/" );
  SearchPath.push_back( "/Developer/" );
  SearchPath.push_back( "/opt/" );
  SearchPath.push_back( "/usr/local/" );

#else

  // linux and other systems
  SearchPath.push_back( "./" );
  std::string homedir;
  if ( itksys::SystemTools::GetEnv ( "HOME", homedir ) )
    {
    SearchPath.push_back( homedir + "/bin/" );
    }

  SearchPath.push_back( "/opt/" );
  SearchPath.push_back( "/usr/local/" );

#endif

  localDebugMacro( << "Default search path: " << SearchPath << std::endl );

  //
  //  Set the ExecutableNames
  //
#if defined(_WIN32)
  ExecutableNames.push_back( "Fiji.app/ImageJ-win64.exe" );
  ExecutableNames.push_back( "Fiji.app/ImageJ-win32.exe" );
  ExecutableNames.push_back( "ImageJ/ImageJ.exe" );
#elif defined(__APPLE__)
  ExecutableNames.push_back( "Fiji.app" );
  ExecutableNames.push_back( "ImageJ/ImageJ64.app" );
  ExecutableNames.push_back( "ImageJ/ImageJ.app" );
#else
  // Linux
#endif

  ImageViewer::FindViewingApplication();

  ViewerImageCount = 0;
  AreDefaultsInitialized = true;
  }

ImageViewer::ImageViewer()
  {

  if (not AreDefaultsInitialized)
    {
    initializeDefaults();
    }

  viewCommand = DefaultViewCommand;
  viewColorCommand = DefaultViewColorCommand;
  fijiCommand = DefaultFijiCommand;

  application = DefaultApplication;

  fileExtension = DefaultFileExtension;

  customCommand = "";
  }


std::string ImageViewer::FindViewingApplication()
  {
  std::string result;
  std::vector<std::string>::iterator name_it;


  for(name_it = ExecutableNames.begin(); name_it != ExecutableNames.end(); name_it++)
    {
#ifdef __APPLE__
      result = itksys::SystemTools::FindDirectory ( (*name_it).c_str(), SearchPath );
      if (!result.length())
        {
        // try looking for a file if no directory
        result = itksys::SystemTools::FindFile ( (*name_it).c_str(), SearchPath );
        }
#else
      result = itksys::SystemTools::FindFile ( (*name_it).c_str(), SearchPath );
#endif
      if (result.length())
        {
        break;
        }
    }

  localDebugMacro( << "FindViewingApplication: " << result << std::endl );
  return result;
  }

const std::vector<std::string>& ImageViewer::GetSearchPath()
  {
  return ImageViewer::SearchPath;
  }

void ImageViewer::SetSearchPath( const std::vector<std::string> & path )
  {
  ImageViewer::SearchPath = path;
  ImageViewer::DefaultApplication = FindViewingApplication();
  }

const std::vector<std::string>& ImageViewer::GetExecutableNames()
  {
  return ImageViewer::ExecutableNames;
  }

void ImageViewer::SetExecutableNames( const std::vector<std::string> & names )
  {
  ImageViewer::ExecutableNames = names;
  ImageViewer::DefaultApplication = FindViewingApplication();
  }

void ImageViewer::SetCommand(const std::string & command )
  {
  customCommand = command;
  }

const std::string & ImageViewer::GetCommand() const
  {
  if (customCommand.length() == 0)
    {
    return viewCommand;
    }
  return customCommand;
  }

void ImageViewer::SetFileExtension(const std::string & ext )
  {
  fileExtension = ext;
  }

const std::string & ImageViewer::GetFileExtension() const
  {
  return fileExtension;
  }

void ImageViewer::SetDebug( const bool dbg )
  {
  DebugOn = dbg;
  }

bool ImageViewer::GetDebug()
  {
  return DebugOn;
  }

void ImageViewer::SetTitle(const std::string & t )
  {
  title = t;
  }

const std::string & ImageViewer::GetTitle() const
  {
  return title;
  }

void ImageViewer::SetApplication( const std::string & app )
  {
  application = app;
  }

const std::string & ImageViewer::GetApplication() const
  {
  return application;
  }

  } // namespace simple
} // namespace itk
