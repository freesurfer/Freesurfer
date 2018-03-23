#!/usr/bin/env python
# -*- coding: utf-8 -*-

"""The setup script."""
import os
from setuptools import setup, find_packages, Distribution

class BinaryDistribution(Distribution):
    def is_pure(self):
        return False
    def has_ext_modules(self):
        return True

def create_init_file():
    file_path = os.path.join(os.path.dirname(__file__), 'bin','__init__.py')
    if not os.path.exists(file_path):
        with open(file_path, 'w'):
            pass

create_init_file()

setup(
    distclass=BinaryDistribution,
    name='GEMS2Python',
    version='0.1.0',
    description="Python wrapper for GEMS2 C++ tooling",
    package_dir={'gems2python':'bin'},
    packages=['gems2python'],
    package_data={
        'gems2python':['GEMS2Python.cpython-35m-x86_64-linux-gnu.so'],
    },
    zip_safe=False,
    keywords='gems2python samseg gems2 freesurfer',
    classifiers=[
        'Development Status :: 2 - Pre-Alpha',
        'Intended Audience :: Developers',
        'Natural Language :: English',
        'Programming Language :: Python :: 3.5',
    ],
)
