@echo off
setlocal
call "A:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
if errorlevel 1 exit /b 1

set SDKROOT=C:\Program Files (x86)\Audiokinetic\Wwise v2010.3.3 build 3773\SDK
set SDKLIB=%SDKROOT%\x64_vc90\Debug\lib
set OUTDIR=%~dp0Out
if not exist "%OUTDIR%" mkdir "%OUTDIR%"

echo === Compiling Stub.cpp ===
cl.exe /nologo /c /std:c++17 /EHsc /MDd /Zi /Od /DWIN64 /D_WIN32_WINNT=0x0601 /I "%SDKROOT%\include" /Fo"%OUTDIR%\Stub.obj" /Fd"%OUTDIR%\Stub.pdb" "%~dp0Stub.cpp"
if errorlevel 1 exit /b 1

echo === Linking WwiseV56DebugBuild.dll ===
link.exe /nologo /DLL /DEBUG /MACHINE:X64 /NODEFAULTLIB:atlsd.lib /DEF:"%~dp0Generated\WwiseV56Debug.def" /OUT:"%OUTDIR%\WwiseV56DebugBuild.dll" /PDB:"%OUTDIR%\WwiseV56DebugBuild.pdb" ^
  "%OUTDIR%\Stub.obj" ^
  "%SDKLIB%\AkSoundEngine.lib" ^
  "%SDKLIB%\AkMusicEngine.lib" ^
  "%SDKLIB%\AkStreamMgr.lib" ^
  "%SDKLIB%\AkMemoryMgr.lib" ^
  "%SDKLIB%\CommunicationCentral.lib" ^
  "%SDKLIB%\AkAudioInputSource.lib" ^
  "%SDKLIB%\AkCompressorFX.lib" ^
  "%SDKLIB%\AkConvolutionReverbFX.lib" ^
  "%SDKLIB%\AkDBox.lib" ^
  "%SDKLIB%\AkDBoxPitchRollHeave.lib" ^
  "%SDKLIB%\AkDelayFX.lib" ^
  "%SDKLIB%\AkExpanderFX.lib" ^
  "%SDKLIB%\AkFlangerFX.lib" ^
  "%SDKLIB%\AkGuitarDistortionFX.lib" ^
  "%SDKLIB%\AkMP3Source.lib" ^
  "%SDKLIB%\AkMatrixReverbFX.lib" ^
  "%SDKLIB%\AkMeterFX.lib" ^
  "%SDKLIB%\AkMotionGenerator.lib" ^
  "%SDKLIB%\AkParametricEQFX.lib" ^
  "%SDKLIB%\AkPeakLimiterFX.lib" ^
  "%SDKLIB%\AkRoomVerbFX.lib" ^
  "%SDKLIB%\AkRumble.lib" ^
  "%SDKLIB%\AkSilenceSource.lib" ^
  "%SDKLIB%\AkSineSource.lib" ^
  "%SDKLIB%\AkSoundSeedImpactFX.lib" ^
  "%SDKLIB%\AkSoundSeedWind.lib" ^
  "%SDKLIB%\AkSoundSeedWoosh.lib" ^
  "%SDKLIB%\AkToneSource.lib" ^
  "%SDKLIB%\AkTremoloFX.lib" ^
  "%SDKLIB%\AkVorbisDecoder.lib" ^
  "%SDKLIB%\McDSPFutzBoxFX.lib" ^
  "%SDKLIB%\McDSPLimiterFX.lib" ^
  ws2_32.lib dxguid.lib dsound.lib winmm.lib msacm32.lib Xinput9_1_0.lib ^
  legacy_stdio_definitions.lib legacy_stdio_wide_specifiers.lib
if errorlevel 1 exit /b 1

echo === Done: %OUTDIR%\WwiseV56DebugBuild.dll ===
endlocal
