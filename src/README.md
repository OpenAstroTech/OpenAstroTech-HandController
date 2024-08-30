# OpenAstroTracker-HandController
Official firmware for the OpenAstroTracker Hand Controller. Other important resources can be found [here](https://wiki.openastrotech.com/en/Links).

## Change log
See the [Changelog](Changelog.md) for details about what versions made what changes.

## Coding guidelines

See `.clang-format` file. A GitHub action is run on every PR to make sure that the code complies with the formatting guidelines.

### Run clang-format locally
* Install `clang-format` version 12. _Note: not all distributions default to version 12_
  * Windows: Installers available from [LLVM Website](https://llvm.org/builds/)
  * Ubuntu: `sudo apt install clang-format-12`
  * ArchLinux: `sudo pacman -S clang`
* Run the formatter: 
  * VSCode Extension: [https://marketplace.visualstudio.com/items?itemName=xaver.clang-format](https://marketplace.visualstudio.com/items?itemName=xaver.clang-format)
  * Shell: `bash -c 'shopt -s nullglob globstar;GLOBIGNORE=./src/libs/TimerInterrupt/*; for i in ./{.,src/**,unit_tests,boards/**}/*.{c,cpp,h,hpp}; do clang-format -i $i; done'`
## Contribution

This is an open source project and everyone is welcome to contribute. We will be following these rules while reviewing your pull request:
- The pull request consists **only** of the **changes related to its particular feature or bugfix**. If there are multiple unrelated changes which should be merged into this repository, you have to create a separate pull request for each of them. 
- The pull request **builds correctly**. If it doesn't, please fix the issues and push them to the source branch. You can use the matrix_build.py script to build all the important configurations locally (works similar to our CI).
- The pull request can only be merged **after** all comments were resolved BY **OAT DEVELOPERS**. Please don't resolve the comments yourself since this can lead to missed issues.
- If the pull request is not maintained by its author in a reasonably prompt manner after a review, the developers can decide to close it without merging since the accumulated merge conflicts and original code changes could lead to massive efforts. You can then still recreate your pull request after applying all the required changes on your fork branch.

## Development

You must use VSCode with [PlatformIO](https://platformio.org/) for development. It allows automatic dependency management, powerful IDE, debugging, automatic build flags definition and more.


