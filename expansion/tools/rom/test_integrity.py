"""Permanent validator controls: distinguish useful warnings from false rules."""
import unittest
import numpy as np
from cyclic_integrity import metrics
class IntegrityControls(unittest.TestCase):
 def test_periodic_sine_does_not_need_duplicate_endpoint(self):
  a=np.sin(2*np.pi*np.arange(256)/256)
  self.assertNotEqual(a[0],a[-1]);self.assertFalse(metrics(a)['suspicious'])
 def test_isolated_bad_ramp_is_reported(self):self.assertTrue(metrics(np.linspace(-.8,.8,256))['suspicious'])
 def test_valid_coherent_harmonics_can_warn(self):
  a=.3*sum(np.sin(2*np.pi*k*np.arange(256)/256)/k for k in range(1,48))
  self.assertTrue(metrics(a)['suspicious'])
  self.assertAlmostEqual((a[1]-a[0]),(a[0]-a[-1]),places=12)
 def test_intentional_square_edge_is_not_a_zero_crossing_requirement(self):
  a=np.where(np.arange(256)<128,.8,-.8);self.assertTrue(metrics(a)['suspicious'])
if __name__=='__main__':unittest.main()
