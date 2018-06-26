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
    if (debugOn)                                                        \
      {                                                                 \
      std::ostringstream msg;                                           \
      msg << "Debug: In " __FILE__ ", line " << __LINE__ << ": " x      \
          << "\n\n";                                                    \
      ::itk::OutputWindowDisplayDebugText( msg.str().c_str() );         \
      }                                                                 \
  }                                                                     \

namespace itk
{
  namespace simple
  {

int ImageViewer::ViewerImageCount=0;
bool ImageViewer::AreDefaultsInitialized=false;

void ImageViewer::initializeDefaults()
  {
  if (AreDefaultsInitialized)
    return;

  // check environment variables for user specified strings

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
  fijiViewCommand = DefaultFijiViewCommand;


  FindViewingApplication();
  customCommand = "";
  }


void ImageViewer::FindViewingApplication()
  {
  std::string dir, name;
  std::vector<std::string>::iterator dir_it, name_it;

  for(dir_it = SearchPath.begin(); dir_it != SearchPath.end(); dir_it++)
    {
    for(name_it = ExecutableNames.begin(); name_it != ExecutableNames.end(); name_it++)
      {
      }
    }
  }

const std::vector<std::string>& ImageViewer::GetSearchPath()
  {
  }

void ImageViewer::SetSearchPath( const std::vector<std::string> & path )
  {
  ImageViewer::SearchPath = path;
  FindViewingApplication();
  }

  } // namespace simple
} // namespace itk
