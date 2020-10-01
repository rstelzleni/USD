
* updating python versions requires touching setup.py and build config
* each release needs a specific version, set in setup.py
* license and readme in setup.py is distributed with the package
* docker container is used for linux builds only, give update instructions
* mention docker container is in my namespace
* check with sunya, best place for change in cmake Private.cmake macro
* -for-pypi flag added just for mac linking support. other flags passed at command line
* should we try to build in a better place than the default checkout?
* need to try an older linux base image for older pip support
* windows, do we need x86 and x64 builds?
* check on windows py3.8 issue, seems like ctypes shouldn't matter
* need to update package name, in setup and in pipeline file

## Steps to Publish

From the azure pipeline download the artifact file called dist.zip. On a Linux
host you could publish like so, in the directory with the dist file.

```
unzip dist.zip
python3 -m pip install twine
python3 -m twine upload --repository-url https://test.pypi.org/legacy/ dist/*
```

The name, version number and other properties of your package will be extracted
from the whl files that are uploaded. The above publishes to the test server,
to publish to the main PyPI archive remove the `repository-url` parameter.
