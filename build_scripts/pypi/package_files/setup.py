import setuptools

import glob, os, shutil

# This setup.py script expects to be run from an inst directory in a typical
# USD build run from build_usd.py. USD should be built with the for-pypi flag. 
#
# The output of this setup will be a wheel package that does not work. After
# building the wheel package, you will also need to relocate dynamic library
# dependencies into the package, and you will need to update the pluginfo files
# to point to the new locations of those shared library dependencies. How this
# is done depends on platform, and is mostly accomplished by steps in the CI
# system.

WORKING_ROOT = '.'
USD_BUILD_OUTPUT = os.path.join(WORKING_ROOT, 'inst/')
BUILD_DIR = os.path.join(WORKING_ROOT, 'pypi/')

# Copy everything in lib over before we start making changes
shutil.copytree(os.path.join(USD_BUILD_OUTPUT, 'lib'), os.path.join(BUILD_DIR, 'lib'))

# Move the pluginfos into a directory contained inside pxr, for easier
# distribution. This breaks the relative paths in the pluginfos, but we'll need
# to update them later anyway after running "auditwheel repair", which will
# move the libraries to a new directory
shutil.move(os.path.join(BUILD_DIR, 'lib/usd'), os.path.join(BUILD_DIR, 'lib/python/pxr/pluginfo'))

# Get the readme text
with open("README.md", "r") as fh:
    long_description = fh.read()

# Config
setuptools.setup(
    name="example-pxr-usd-from-nvidia",
    version="0.0.8",
    author="Ryan Stelzleni",
    author_email="rstelzleni@nvidia.com",
    description="Pixar's Universal Scene Description library",
    long_description=long_description,
    long_description_content_type="text/markdown",
    url="https://github.com/PixarAnimationStudios/USD",
    project_urls={
        "Documentation": "https://graphics.pixar.com/usd/docs/USD-Developer-API-Reference.html",
        "Developer Docs": "https://graphics.pixar.com/usd/docs/USD-Developer-API-Reference.html",
        "Source":"https://github.com/PixarAnimationStudios/USD",
        "Helpful Links": "https://github.com/vfxpro99/usd-resources"
    },
    packages=setuptools.find_packages(os.path.join(BUILD_DIR, 'lib/python')),
    package_dir={"": os.path.join(BUILD_DIR, 'lib/python')},
    package_data={
        "": ["*.so"],
        "pxr": ["pluginfo/*", "pluginfo/*/*", "pluginfo/*/*/*"],
    },
    classifiers=[
        "Programming Language :: Python :: 3",
        "License :: Other/Proprietary License",
        "Operating System :: POSIX :: Linux",
        "Operating System :: MacOS :: MacOS X",
        "Operating System :: Microsoft :: Windows :: Windows 10",
        "Environment :: Console",
        "Topic :: Multimedia :: Graphics",
    ],
    python_requires='>=3.6, <3.9',
)
