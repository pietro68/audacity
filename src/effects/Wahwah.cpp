/**********************************************************************

  Audacity: A Digital Audio Editor

  Wahwah.cpp

  Effect programming:
  Nasca Octavian Paul (Paul Nasca)

  UI programming:
  Dominic Mazzoni (with the help of wxDesigner)
  Vaughan Johnson (Preview)

*******************************************************************//**

\class EffectWahwah
\brief An Effect that adds a 'spectral glide'.

*//*******************************************************************/


#include "Wahwah.h"
#include "LoadEffects.h"

#include <math.h>

#include <wx/intl.h>
#include <wx/slider.h>

#include "../ShuttleGui.h"
#include "../widgets/valnum.h"

enum
{
   ID_Freq = 10000,
   ID_Phase,
   ID_Depth,
   ID_Res,
   ID_FreqOfs,
   ID_OutGain
};

const EffectParameterMethods& EffectWahwah::Parameters() const
{
   static CapturedParameters<EffectWahwah,
      Freq, Phase, Depth, Res, FreqOfs, OutGain
   > parameters;
   return parameters;
}

// How many samples are processed before recomputing the lfo value again
#define lfoskipsamples 30

//
// EffectWahwah
//

const ComponentInterfaceSymbol EffectWahwah::Symbol
{ XO("Wahwah") };

namespace{ BuiltinEffectsModule::Registration< EffectWahwah > reg; }

BEGIN_EVENT_TABLE(EffectWahwah, wxEvtHandler)
    EVT_SLIDER(ID_Freq, EffectWahwah::OnFreqSlider)
    EVT_SLIDER(ID_Phase, EffectWahwah::OnPhaseSlider)
    EVT_SLIDER(ID_Depth, EffectWahwah::OnDepthSlider)
    EVT_SLIDER(ID_Res, EffectWahwah::OnResonanceSlider)
    EVT_SLIDER(ID_FreqOfs, EffectWahwah::OnFreqOffSlider)
    EVT_SLIDER(ID_OutGain, EffectWahwah::OnGainSlider)
    EVT_TEXT(ID_Freq, EffectWahwah::OnFreqText)
    EVT_TEXT(ID_Phase, EffectWahwah::OnPhaseText)
    EVT_TEXT(ID_Depth, EffectWahwah::OnDepthText)
    EVT_TEXT(ID_Res, EffectWahwah::OnResonanceText)
    EVT_TEXT(ID_FreqOfs, EffectWahwah::OnFreqOffText)
    EVT_TEXT(ID_OutGain, EffectWahwah::OnGainText)
END_EVENT_TABLE();



EffectWahwah::EffectWahwah()
{
   SetLinearEffectFlag(true);
}

EffectWahwah::~EffectWahwah()
{
}

// ComponentInterface implementation

ComponentInterfaceSymbol EffectWahwah::GetSymbol() const
{
   return Symbol;
}

TranslatableString EffectWahwah::GetDescription() const
{
   return XO("Rapid tone quality variations, like that guitar sound so popular in the 1970's");
}

ManualPageID EffectWahwah::ManualPage() const
{
   return L"Wahwah";
}

// EffectDefinitionInterface implementation

EffectType EffectWahwah::GetType() const
{
   return EffectTypeProcess;
}

bool EffectWahwah::SupportsRealtime() const
{
   return true;
}

unsigned EffectWahwah::GetAudioInCount() const
{
   return 1;
}

unsigned EffectWahwah::GetAudioOutCount() const
{
   return 1;
}

bool EffectWahwah::ProcessInitialize(
   EffectSettings &, sampleCount, ChannelNames chanMap)
{
   InstanceInit(mMaster, mSampleRate);

   if (chanMap[0] == ChannelNameFrontRight)
   {
      mMaster.phase += M_PI;
   }

   return true;
}

size_t EffectWahwah::ProcessBlock(EffectSettings &settings,
   const float *const *inBlock, float *const *outBlock, size_t blockLen)
{
   return InstanceProcess(settings, mMaster, inBlock, outBlock, blockLen);
}

bool EffectWahwah::RealtimeInitialize(EffectSettings &)
{
   SetBlockSize(512);

   mSlaves.clear();

   return true;
}

bool EffectWahwah::RealtimeAddProcessor(
   EffectSettings &settings, unsigned, float sampleRate)
{
   EffectWahwahState slave;

   InstanceInit(slave, sampleRate);

   mSlaves.push_back(slave);

   return true;
}

bool EffectWahwah::RealtimeFinalize(EffectSettings &) noexcept
{
   mSlaves.clear();

   return true;
}

size_t EffectWahwah::RealtimeProcess(int group, EffectSettings &settings,
   const float *const *inbuf, float *const *outbuf, size_t numSamples)
{
   return InstanceProcess(settings, mSlaves[group], inbuf, outbuf, numSamples);
}

// Effect implementation

std::unique_ptr<EffectUIValidator>
EffectWahwah::PopulateOrExchange(ShuttleGui & S, EffectSettingsAccess &)
{
   auto& ms = mSettings;

   S.SetBorder(5);
   S.AddSpace(0, 5);

   S.StartMultiColumn(3, wxEXPAND);
   {
      S.SetStretchyCol(2);
   
      mFreqT = S.Id(ID_Freq)
         .Validator<FloatingPointValidator<double>>(
            5, &ms.mFreq, NumValidatorStyle::ONE_TRAILING_ZERO, Freq.min, Freq.max)
         .AddTextBox(XXO("LFO Freq&uency (Hz):"), L"", 12);

      mFreqS = S.Id(ID_Freq)
         .Name(XO("LFO frequency in hertz"))
         .Style(wxSL_HORIZONTAL)
         .MinSize( { 100, -1 } )
         .AddSlider( {}, Freq.def * Freq.scale, Freq.max * Freq.scale, Freq.min * Freq.scale);

      mPhaseT = S.Id(ID_Phase)
         .Validator<FloatingPointValidator<double>>(
            1, &ms.mPhase, NumValidatorStyle::DEFAULT, Phase.min, Phase.max)
         .AddTextBox(XXO("LFO Sta&rt Phase (deg.):"), L"", 12);

      mPhaseS = S.Id(ID_Phase)
         .Name(XO("LFO start phase in degrees"))
         .Style(wxSL_HORIZONTAL)
         .MinSize( { 100, -1 } )
         .AddSlider( {}, Phase.def * Phase.scale, Phase.max * Phase.scale, Phase.min * Phase.scale);
      mPhaseS->SetLineSize(10);

      mDepthT = S.Id(ID_Depth)
         .Validator<IntegerValidator<int>>(
            &ms.mDepth, NumValidatorStyle::DEFAULT, Depth.min, Depth.max)
         .AddTextBox(XXO("Dept&h (%):"), L"", 12);

      mDepthS = S.Id(ID_Depth)
         .Name(XO("Depth in percent"))
         .Style(wxSL_HORIZONTAL)
         .MinSize( { 100, -1 } )
         .AddSlider( {}, Depth.def * Depth.scale, Depth.max * Depth.scale, Depth.min * Depth.scale);

      mResT = S.Id(ID_Res)
         .Validator<FloatingPointValidator<double>>(
            1, &ms.mRes, NumValidatorStyle::DEFAULT, Res.min, Res.max)
         .AddTextBox(XXO("Reso&nance:"), L"", 12);

      mResS = S.Id(ID_Res)
         .Name(XO("Resonance"))
         .Style(wxSL_HORIZONTAL)
         .MinSize( { 100, -1 } )
         .AddSlider( {}, Res.def * Res.scale, Res.max * Res.scale, Res.min * Res.scale);

      mFreqOfsT = S.Id(ID_FreqOfs)
         .Validator<IntegerValidator<int>>(
            &ms.mFreqOfs, NumValidatorStyle::DEFAULT, FreqOfs.min, FreqOfs.max)
         .AddTextBox(XXO("Wah Frequency Offse&t (%):"), L"", 12);

      mFreqOfsS = S.Id(ID_FreqOfs)
         .Name(XO("Wah frequency offset in percent"))
         .Style(wxSL_HORIZONTAL)
         .MinSize( { 100, -1 } )
         .AddSlider( {}, FreqOfs.def * FreqOfs.scale, FreqOfs.max * FreqOfs.scale, FreqOfs.min * FreqOfs.scale);

      mOutGainT = S.Id(ID_OutGain)
         .Validator<FloatingPointValidator<double>>(
            1, &ms.mOutGain, NumValidatorStyle::DEFAULT, OutGain.min, OutGain.max)
         .AddTextBox(XXO("&Output gain (dB):"), L"", 12);

      mOutGainS = S.Id(ID_OutGain)
         .Name(XO("Output gain (dB)"))
         .Style(wxSL_HORIZONTAL)
         .MinSize( { 100, -1 } )
         .AddSlider( {}, OutGain.def * OutGain.scale, OutGain.max * OutGain.scale, OutGain.min * OutGain.scale);
   }
   S.EndMultiColumn();
   return nullptr;
}

bool EffectWahwah::TransferDataToWindow(const EffectSettings &)
{
   auto& ms = mSettings;

   mFreqS->SetValue((int) (ms.mFreq * Freq.scale));
   mPhaseS->SetValue((int) (ms.mPhase * Phase.scale));
   mDepthS->SetValue((int) (ms.mDepth * Depth.scale));
   mResS->SetValue((int) (ms.mRes * Res.scale));
   mFreqOfsS->SetValue((int) (ms.mFreqOfs * FreqOfs.scale));
   mOutGainS->SetValue((int) (ms.mOutGain * OutGain.scale));

   return true;
}

// EffectWahwah implementation

void EffectWahwah::InstanceInit(EffectWahwahState & data, float sampleRate)
{
   auto& ms = mSettings;

   data.samplerate = sampleRate;
   data.lfoskip = ms.mFreq * 2 * M_PI / sampleRate;
   data.skipcount = 0;
   data.xn1 = 0;
   data.xn2 = 0;
   data.yn1 = 0;
   data.yn2 = 0;
   data.b0 = 0;
   data.b1 = 0;
   data.b2 = 0;
   data.a0 = 0;
   data.a1 = 0;
   data.a2 = 0;

   data.depth = ms.mDepth / 100.0;
   data.freqofs = ms.mFreqOfs / 100.0;
   data.phase = ms.mPhase * M_PI / 180.0;
   data.outgain = DB_TO_LINEAR(ms.mOutGain);
}

size_t EffectWahwah::InstanceProcess(EffectSettings &settings,
   EffectWahwahState & data,
   const float *const *inBlock, float *const *outBlock, size_t blockLen)
{
   auto& ms = mSettings;

   const float *ibuf = inBlock[0];
   float *obuf = outBlock[0];
   double frequency, omega, sn, cs, alpha;
   double in, out;

   data.lfoskip = ms.mFreq * 2 * M_PI / data.samplerate;
   data.depth = ms.mDepth / 100.0;
   data.freqofs = ms.mFreqOfs / 100.0;

   data.phase = ms.mPhase * M_PI / 180.0;
   data.outgain = DB_TO_LINEAR(ms.mOutGain);

   for (decltype(blockLen) i = 0; i < blockLen; i++)
   {
      in = (double) ibuf[i];

      if ((data.skipcount++) % lfoskipsamples == 0)
      {
         frequency = (1 + cos(data.skipcount * data.lfoskip + data.phase)) / 2;
         frequency = frequency * data.depth * (1 - data.freqofs) + data.freqofs;
         frequency = exp((frequency - 1) * 6);
         omega = M_PI * frequency;
         sn = sin(omega);
         cs = cos(omega);
         alpha = sn / (2 * ms.mRes);
         data.b0 = (1 - cs) / 2;
         data.b1 = 1 - cs;
         data.b2 = (1 - cs) / 2;
         data.a0 = 1 + alpha;
         data.a1 = -2 * cs;
         data.a2 = 1 - alpha;
      };
      out = (data.b0 * in + data.b1 * data.xn1 + data.b2 * data.xn2 - data.a1 * data.yn1 - data.a2 * data.yn2) / data.a0;
      data.xn2 = data.xn1;
      data.xn1 = in;
      data.yn2 = data.yn1;
      data.yn1 = out;
      out *= data.outgain;

      obuf[i] = (float) out;
   }

   return blockLen;
}

void EffectWahwah::OnFreqSlider(wxCommandEvent & evt)
{
   auto& ms = mSettings;

   ms.mFreq = (double) evt.GetInt() / Freq.scale;
   mFreqT->GetValidator()->TransferToWindow();
   EnableApply(mUIParent->Validate());
}

void EffectWahwah::OnPhaseSlider(wxCommandEvent & evt)
{
   auto& ms = mSettings;

   int val = ((evt.GetInt() + 5) / 10) * 10; // round to nearest multiple of 10
   val = val > Phase.max * Phase.scale ? Phase.max * Phase.scale : val;
   mPhaseS->SetValue(val);
   ms.mPhase = (double) val / Phase.scale;
   mPhaseT->GetValidator()->TransferToWindow();
   EnableApply(mUIParent->Validate());
}

void EffectWahwah::OnDepthSlider(wxCommandEvent & evt)
{
   auto& ms = mSettings;

   ms.mDepth = evt.GetInt() / Depth.scale;
   mDepthT->GetValidator()->TransferToWindow();
   EnableApply(mUIParent->Validate());
}

void EffectWahwah::OnResonanceSlider(wxCommandEvent & evt)
{
   auto& ms = mSettings;

   ms.mRes = (double) evt.GetInt() / Res.scale;
   mResT->GetValidator()->TransferToWindow();
   EnableApply(mUIParent->Validate());
}

void EffectWahwah::OnFreqOffSlider(wxCommandEvent & evt)
{
   auto& ms = mSettings;

   ms.mFreqOfs = evt.GetInt() / FreqOfs.scale;
   mFreqOfsT->GetValidator()->TransferToWindow();
   EnableApply(mUIParent->Validate());
}

void EffectWahwah::OnGainSlider(wxCommandEvent & evt)
{
   auto& ms = mSettings;

   ms.mOutGain = evt.GetInt() / OutGain.scale;
   mOutGainT->GetValidator()->TransferToWindow();
   EnableApply(mUIParent->Validate());
}

void EffectWahwah::OnFreqText(wxCommandEvent & WXUNUSED(evt))
{
   auto& ms = mSettings;

   if (!EnableApply(mUIParent->TransferDataFromWindow()))
   {
      return;
   }

   mFreqS->SetValue((int) (ms.mFreq * Freq.scale));
}

void EffectWahwah::OnPhaseText(wxCommandEvent & WXUNUSED(evt))
{
   auto& ms = mSettings;

   if (!EnableApply(mUIParent->TransferDataFromWindow()))
   {
      return;
   }

   mPhaseS->SetValue((int) (ms.mPhase * Phase.scale));
}

void EffectWahwah::OnDepthText(wxCommandEvent & WXUNUSED(evt))
{
   auto& ms = mSettings;

   if (!EnableApply(mUIParent->TransferDataFromWindow()))
   {
      return;
   }

   mDepthS->SetValue((int) (ms.mDepth * Depth.scale));
}

void EffectWahwah::OnResonanceText(wxCommandEvent & WXUNUSED(evt))
{
   auto& ms = mSettings;

   if (!EnableApply(mUIParent->TransferDataFromWindow()))
   {
      return;
   }

   mResS->SetValue((int) (ms.mRes * Res.scale));
}

void EffectWahwah::OnFreqOffText(wxCommandEvent & WXUNUSED(evt))
{
   auto& ms = mSettings;

   if (!EnableApply(mUIParent->TransferDataFromWindow()))
   {
      return;
   }

   mFreqOfsS->SetValue((int) (ms.mFreqOfs * FreqOfs.scale));
}

void EffectWahwah::OnGainText(wxCommandEvent & WXUNUSED(evt))
{
   auto& ms = mSettings;

   if (!EnableApply(mUIParent->TransferDataFromWindow()))
   {
      return;
   }

   mOutGainS->SetValue((int) (ms.mOutGain * OutGain.scale));
}
