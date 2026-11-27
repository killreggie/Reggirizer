REGGIRIZER - building and installing on Windows for FL Studio
=============================================================

1. Install Visual Studio 2022 Community (free):
   https://visualstudio.microsoft.com/vs/community/
   In the installer, tick "Desktop development with C++" and install.

2. Unzip this folder anywhere (for example your Desktop).

3. Double-click build-windows.bat
   - The first build takes a few minutes.
   - At the end it asks for admin permission to copy the plugin into
     C:\Program Files\Common Files\VST3

4. In FL Studio:
   Options > Manage plugins > Find plugins (Verify plugins)
   Then on your vocal's mixer track, click an empty insert slot and pick Reggirizer.

If the automatic install fails, copy this folder yourself:
   build-win\Reggirizer_artefacts\Release\VST3\Reggirizer.vst3
into:
   C:\Program Files\Common Files\VST3
