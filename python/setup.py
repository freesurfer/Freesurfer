#!/usr/bin/env python

import sys
import glob
import platform
import os.path as op
from setuptools import setup, find_packages, Distribution


# the freesurfer python packages
packages = [
    'freesurfer',
    'freesurfer.algorithm',
    'freesurfer.gems',
    'freesurfer.samseg'
]

# get required dependencies from requirements.txt
with open(op.join(op.dirname(op.realpath(__file__)), 'requirements.txt')) as file:
    requirements = [line for line in file.read().splitlines() if not line.startswith('#')]

# ---- run the setup ----

# since the freesurfer package will contain python-wrapped c++ libraries, we need to indicate
# that it will be a platform-dependent distribution via a custom Distribution class
class BinaryDistribution(Distribution):
    def is_pure(self):
        return False
    def has_ext_modules(self):
        return True

# locates cpython libraries compiled with pybind
def find_shared_libs(libname):
    libraries = glob.glob('**/%s.*%s*.so' % (libname, platform.system().lower()), recursive=True)
    if not libraries:
        print('error: could not find %s library that matches the current python version' % libname)
        sys.exit(1)
    return [op.basename(filename) for filename in libraries]

setup(
    distclass=BinaryDistribution,
    name='freesurfer',
    version='0.0.1',
    description='Python package for FreeSurfer neuroimaging software',
    author='Laboratory for Computational Neuroimaging',
    author_email='freesurfer@nmr.mgh.harvard.edu',
    url='https://github.com/freesurfer/freesurfer',
    packages=find_packages(include=packages),
    package_data={'freesurfer.gems': find_shared_libs('gems_python'),
                  'freesurfer.algorithm': find_shared_libs('algorithm_python')},
    install_requires=requirements,
    include_package_data=True
)
