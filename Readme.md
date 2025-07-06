
# The Desktop Trojan

```shell
git clone --recurse-submodules https://github.com/gaziduc/the-desktop-trojan.git
cd the-desktop-trojan
```

Open the project with IntelliJ Idea, and change the CMake "Build Type" to "Release" in Settings > Build, Execution and Deployment > CMake. If you don't do this the harfbuzz library linking will take forever and will fail.
 
Then download all submodules in not already done (it's already done if you've cloned the project with the --recurse-submodules option)

Windows (Powershell) Only:
```shell
 .\submodules\SDL_ttf\external\Get-GitModules.ps1
 .\submodules\SDL_mixer\external\Get-GitModules.ps1
 .\submodules\SDL_image\external\Get-GitModules.ps1
```

Or for Bash users:
```shell
 ./submodules/SDL_ttf/external/download.sh
 ./submodules/SDL_mixer/external/download.sh
 ./submodules/SDL_image/external/download.sh
```

Then you can build!