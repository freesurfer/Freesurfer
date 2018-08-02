#!/usr/bin/env python
import sys, os.path as op
sys.path.append(op.join(op.dirname(sys.argv[0]), '../python'))
import freesurfer.test as fst

rt = fst.RegressionTest()

# always run a multithreaded test, but generate single-threaded reference data
if rt.regenerate: threads = 1
else: threads = 8

rt.run('mris_sphere -seed 1234 rh.inflated rh.sphere', threads=threads)
rt.surfdiff('rh.sphere', 'rh.ref.sphere')

rt.cleanup()
