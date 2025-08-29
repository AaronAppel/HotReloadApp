call BuildBgfxSolutionTools.bat

set msbuildPath="C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
set vcxProjectPath="Plugin\Plugin.vcxproj"

rem msbuild MySolution.sln /t:MyProjectName /p:Configuration=Debug /p:Platform="Any CPU"
rem msbuild MyProject\MyProject.vcxproj /p:Configuration=Release
%msbuildPath% %vcxProjectPath% /p:Configuration=Debug /p:Platform="x64"

rem IF !errorlevel! NEQ 0 (goto error)

IF EXIST "B:\HotReloadApp\Plugin\x64\Debug\Plugin.dll" (
	goto :end
)

:error
echo May have encountered error(s)
pause
exit /B 1

:end
echo Successfully built Plugin.dll
rem pause
echo Copying Plugin.dll
rem F| to specify as a file and avoid prompting user for 'F' or 'D'
F|xcopy "B:\HotReloadApp\Plugin\x64\Debug\Plugin.dll" "B:\HotReloadApp\x64\Debug\Plugin.dll" /Y