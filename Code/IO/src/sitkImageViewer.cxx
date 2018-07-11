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


namespace
{
  // forward declaration of some helper functions
  std::string ReplaceWords( const std::string command, const std::string app, const std::string filename,
                            const std::string title, bool& fileFlag );
  std::string UnquoteWord( const std::string word );
  std::vector<std::string> ConvertCommand( const std::string command, const std::string app, const std::string filename,
                                           const std::string title="" );
  std::string FormatFileName ( const std::string TempDirectory, const std::string name, const std::string extension,
                               const int tagID );
  std::string DoubleBackslashes( const std::string word );
  std::string BuildFullFileName( const std::string name, const std::string extension, const int tagID );
}

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


//
// A bunch of Set/Get methods for the class
//

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


//
// Helper string, file name, and command string functions
//

namespace
{

// Function to replace %tokens in a string.  The tokens are %a and %f for
// application and file name respectively.  %% will send % to the output string.
// Multiple occurrences of a token are allowed.
//
std::string ReplaceWords( const std::string command,  const std::string app,  const std::string filename,
                          const std::string title, bool& fileFlag )
  {
  std::string result;

  unsigned int i=0;
  while (i<command.length())
    {

    if (command[i] != '%')
      {
      result.push_back(command[i]);
      }
    else
      {
      if (i<command.length()-1)
        {
        // check the next character after the %
        switch(command[i+1])
          {
          case '%':
            // %%
            result.push_back(command[i]);
            i++;
            break;
          case 'a':
            // %a for application
            if (!app.length())
              {
              sitkExceptionMacro( "No ImageJ/Fiji application found." )
              }
            result.append(app);
            i++;
            break;
          case 't':
            // %t for title
            result.append(title);
            i++;
            break;
          case 'f':
            // %f for filename
            result.append(filename);
            fileFlag = true;
            i++;
            break;
          }

        }
      else
        {
        // if the % is the last character in the string just pass it through
        result.push_back(command[i]);
        }
      }
    i++;
    }
  return result;
  }

// Function to strip the matching leading and trailing quotes off a string
// if there are any.  We need to do this because the way arguments are passed
// to itksysProcess_Execute
//
std::string UnquoteWord( const std::string word )
  {
  size_t l = word.length();

  if (l<2)
    {
    return word;
    }

  switch(word[0])
    {

    case '\'':
    case '\"':
      if (word[l-1] == word[0])
        {
        return word.substr(1, l-2);
        }
      else
        {
        return word;
        }
      break;

    default:
      return word;
      break;
    }
  }

std::vector<std::string> ConvertCommand( const std::string command, const std::string app, const std::string filename,
                                         const std::string title )
  {

  std::string t;
  if (title == "")
    {
    t = filename;
    }
  else
    {
    t = title;
    }

  bool fileFlag=false;
  std::string new_command = ReplaceWords(command, app, filename, t, fileFlag);
  std::istringstream iss(new_command);
  std::vector<std::string> result;
  std::vector<unsigned char> quoteStack;
  std::string word;
  unsigned int i=0;

  while (i<new_command.length())
    {
    switch (new_command[i])
      {

      case '\'':
      case '\"':
        word.push_back(new_command[i]);
        if (quoteStack.size())
          {
          if (new_command[i] == quoteStack[quoteStack.size()-1])
            {
            // we have a matching pair, so pop it off the stack
            quoteStack.pop_back();
            }
          else
            {
            // the top of the stack and the new one don't match, so push it on
            quoteStack.push_back(new_command[i]);
            }
          }
        else
          {
          // quoteStack is empty so push this new quote on the stack.
          quoteStack.push_back(new_command[i]);
          }
        break;

      case ' ':
        if (quoteStack.size())
          {
          // the space occurs inside a quote, so tack it onto the current word.
          word.push_back(new_command[i]);
          }
        else
          {
          // the space isn't inside a quote, so we've ended a word.
          word = UnquoteWord(word);
          result.push_back(word);
          word.clear();
          }
        break;

      default:
        word.push_back(new_command[i]);
        break;
      }
    i++;

    }

  if (word.length())
    {
    word = UnquoteWord(word);
    result.push_back(word);
    }


  // if the filename token wasn't found in the command string, add the filename to the end of the command vector.
  if (!fileFlag)
    {
    result.push_back(filename);
    }
  return result;
  }

//
std::string FormatFileName ( const std::string TempDirectory, const std::string name, const std::string extension,
                             const int tagID )
  {
  std::string TempFile = TempDirectory;

#ifdef _WIN32
  int pid = _getpid();
#else
  int pid = getpid();
#endif

  std::ostringstream tmp;

  if ( name != "" )
    {
    std::string n = name;
    // remove whitespace
    n.erase(std::remove_if(n.begin(), n.end(), &::isspace), n.end());

    tmp << n << "-" << pid << "-" << tagID;
    TempFile = TempFile + tmp.str() + extension;
    }
  else
    {


    tmp << "TempFile-" << pid << "-" << tagID;
    TempFile = TempFile + tmp.str() + extension;
    }
  return TempFile;
  }

#if defined(_WIN32)
  //
  // Function that converts slashes or backslashes to double backslashes.  We need
  // to do this so the file name is properly parsed by ImageJ if it's used in a macro.
  //
  std::string DoubleBackslashes(const std::string word)
  {
    std::string result;

    for (unsigned int i=0; i<word.length(); i++)
      {
      // put in and extra backslash
      if (word[i] == '\\' || word[i]=='/')
        {
        result.push_back('\\');
        result.push_back('\\');
        }
      else
        {
        result.push_back(word[i]);
        }
      }

    return result;
  }
#endif

//
//
std::string BuildFullFileName(const std::string name, const std::string extension, const int tagID )
  {
  std::string TempDirectory;

#ifdef _WIN32
  if ( !itksys::SystemTools::GetEnv ( "TMP", TempDirectory )
    && !itksys::SystemTools::GetEnv ( "TEMP", TempDirectory )
    && !itksys::SystemTools::GetEnv ( "USERPROFILE", TempDirectory )
    && !itksys::SystemTools::GetEnv ( "WINDIR", TempDirectory ) )
    {
    sitkExceptionMacro ( << "Can not find temporary directory.  Tried TMP, TEMP, USERPROFILE, and WINDIR environment variables" );
    }
  TempDirectory = TempDirectory + "\\";
  TempDirectory = DoubleBackslashes(TempDirectory);
#else
  TempDirectory = "/tmp/";
#endif
  return FormatFileName ( TempDirectory, name, extension, tagID );
  }

}
