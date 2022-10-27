#ifndef _VST_DEBUGTOOLS_H_
#define _VST_DEBUGTOOLS_H_

#include <map>

// Enables printing out to output window (in Visual Studio, but can be probably ported to OSX)
// contents of VSTEffectSettings to help debugging.
//
// How to use:
//
// in VSTEffect.h,       comment out the local definition of VSTEffectSettings and include this file.
// in EffectInterface.h  include this file just after the definition of EffectSettings
//


struct VSTEffectSettings
{
   // These are saved in the Config and checked against when loading a preset, to make sure
   // that we are loading a Config  which is compatible.
   //
   int32_t mUniqueID{};
   int32_t mVersion{};
   int32_t mNumParams{};

   // When loading a preset, the preferred way is to use the chunk; when not present in
   // the Config or failing to load, we fall back to loading single parameters (ID, value) pairs.
   //
   // It looks like a plugin might not support this (if their effFlagsProgramChunks bit is off)
   // If not, then hold an empty string
   //
   wxString mChunk;

   // Fallback data used when the chunk is not available.
   std::map<wxString, std::optional<std::pair<int, double> > > mParamsMap;
};


// Definitions of the printing functions
// So far we have them for windows only, please add them for OSx too
//
#define print0Arg(STR)           _RPT0(0, STR)
#define print1Arg(FORMATSTR,ARG) _RPT1(0, FORMATSTR, ARG)

inline void dbgPrint(const std::string& callSite, const VSTEffectSettings& vstSettings)
{
   print1Arg("\n=========== %s ==============\n\n", callSite.c_str());

   std::string stdchunk = vstSettings.mChunk.ToStdString();

   std::size_t hashOfChunk = std::hash<std::string>{}(stdchunk);

   print1Arg("Hash of chunk: %zu\n", hashOfChunk);

   /* alternatively, print the full chunk. This can be up to 4096 characters for some plugins,
   *  this is why we use the hash of it instead
   * 
   _RPT0(0, "Chunk: ");

   // chunk might be too large for the print function to succeed,
   // break it in segments
   size_t segmentlen = 1024;
   size_t numFullSegments = stdchunk.length() / segmentlen;
   for (size_t i = 0; i < numFullSegments; i++)
   {
      if (i > 0)
         _RPT0(0, "       ");

      _RPT1(0, "%s\n", stdchunk.substr(i * segmentlen, segmentlen).c_str());
   }

   size_t remainder = stdchunk.length() - numFullSegments * segmentlen;
   if (remainder > 0)
      _RPT1(0, "       %s\n", stdchunk.substr(numFullSegments * segmentlen, remainder).c_str());
   */

   print0Arg("\n");


   for (const auto& item : vstSettings.mParamsMap)
   {
      // param index
      if (item.second.has_value())
         print1Arg("%2d ", item.second->first);
      else
         print1Arg("%s ", "?");

      // param name
      print1Arg("%-20s: ", item.first.ToStdString().c_str());

      // param value
      if (item.second.has_value())
         print1Arg("%1.3f\n", item.second->second);
      else
         print1Arg("%s\n", "no val");
   }

   print0Arg("===============================\n\n");
}

inline void dbgPrint(const std::string& callSite, const EffectSettings& settings)
{
   auto pSettings = settings.cast<VSTEffectSettings>();
   assert(pSettings);
   dbgPrint(callSite, *pSettings);
}

#endif // !_VST_DEBUGTOOLS_H_
