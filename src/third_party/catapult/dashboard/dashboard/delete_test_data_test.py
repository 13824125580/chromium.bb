# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import webapp2
import webtest

from dashboard import delete_test_data
from dashboard import testing_common
from dashboard import utils
from dashboard.models import graph_data

# Masters, bots and test names to add to the mock datastore.
_MOCK_DATA = [
    ['ChromiumPerf', 'ChromiumWebkit'],
    ['win7', 'mac'],
    {
        'SunSpider': {
            'Total': {
                't': {},
                't_ref': {},
                't_extwr': {},
            },
            '3d-cube': {'t': {}},
        },
        'moz': {
            'read_op_b': {'r_op_b': {}},
        },
    }
]

_TESTS_WITH_ROWS = [
    'ChromiumPerf/mac/SunSpider/Total/t',
    'ChromiumPerf/mac/SunSpider/3d-cube',
    'ChromiumPerf/mac/moz',
    'ChromiumPerf/win7/SunSpider/Total/t',
    'ChromiumPerf/win7/SunSpider/3d-cube',
    'ChromiumPerf/win7/moz',
]

class DeleteTestDataTest(testing_common.TestCase):

  def setUp(self):
    super(DeleteTestDataTest, self).setUp()
    app = webapp2.WSGIApplication([(
        '/delete_test_data', delete_test_data.DeleteTestDataHandler)])
    self.testapp = webtest.TestApp(app)
    # Ensure multiple tests run.
    delete_test_data._ROWS_TO_DELETE_AT_ONCE = 30

  def _AddMockData(self):
    """Adds sample TestMetadata and Row entities."""
    testing_common.AddTests(*_MOCK_DATA)

    # Add 50 Row entities to some of the tests.
    for test_path in _TESTS_WITH_ROWS:
      testing_common.AddRows(test_path, range(15000, 15100, 2))

  def _AssertExists(self, test_paths):
    for test_path in test_paths:
      test_key = utils.TestKey(test_path)
      if test_path in _TESTS_WITH_ROWS:
        num_rows = graph_data.Row.query(
            graph_data.Row.parent_test == utils.OldStyleTestKey(test_key)
            ).count()
        self.assertEqual(50, num_rows)
      self.assertIsNotNone(test_key.get())

  def _AssertNotExists(self, test_paths):
    for test_path in test_paths:
      test_key = utils.TestKey(test_path)
      num_rows = graph_data.Row.query(
          graph_data.Row.parent_test == utils.OldStyleTestKey(test_key)).count()
      self.assertEqual(0, num_rows)
      self.assertIsNone(test_key.get())

  def testPost_DeleteTraceLevelTest(self):
    self._AddMockData()
    self.testapp.post('/delete_test_data', {
        'pattern': '*/*/*/*/t',
    })
    self.ExecuteTaskQueueTasks(
        '/delete_test_data', delete_test_data._TASK_QUEUE_NAME)
    self._AssertNotExists([
        'ChromiumPerf/mac/SunSpider/Total/t',
        'ChromiumPerf/win7/SunSpider/Total/t',
        'ChromiumPerf/mac/SunSpider/3d-cube/t',
        'ChromiumPerf/win7/SunSpider/3d-cube/t',
    ])
    self._AssertExists([
        'ChromiumPerf/mac/SunSpider/Total',
        'ChromiumPerf/mac/SunSpider/Total/t_ref',
        'ChromiumPerf/mac/SunSpider/Total/t_extwr',
        'ChromiumPerf/mac/SunSpider/3d-cube',
        'ChromiumPerf/mac/moz',
        'ChromiumPerf/mac/moz/read_op_b/r_op_b',
        'ChromiumPerf/win7/SunSpider/Total',
        'ChromiumPerf/win7/SunSpider/Total/t_ref',
        'ChromiumPerf/win7/SunSpider/Total/t_extwr',
        'ChromiumPerf/win7/SunSpider/3d-cube',
        'ChromiumPerf/win7/moz',
        'ChromiumPerf/win7/moz/read_op_b/r_op_b',
    ])

  def testPost_DeleteChartLevelTest(self):
    self._AddMockData()

    self.testapp.post('/delete_test_data', {
        'pattern': '*/*/SunSpider/Total',
    })
    self.ExecuteTaskQueueTasks(
        '/delete_test_data', delete_test_data._TASK_QUEUE_NAME)
    self._AssertNotExists([
        'ChromiumPerf/mac/SunSpider/Total/t',
        'ChromiumPerf/win7/SunSpider/Total/t',
        'ChromiumPerf/mac/SunSpider/Total/t_ref',
        'ChromiumPerf/win7/SunSpider/Total/t_ref',
        'ChromiumPerf/mac/SunSpider/Total/t_extwr',
        'ChromiumPerf/win7/SunSpider/Total/t_extwr',
        'ChromiumPerf/mac/SunSpider/Total',
        'ChromiumPerf/win7/SunSpider/Total',
    ])
    self._AssertExists([
        'ChromiumPerf/mac/SunSpider/3d-cube/t',
        'ChromiumPerf/win7/SunSpider/3d-cube/t',
    ])


  def testPost_DeleteSuiteLevelTest(self):
    self._AddMockData()

    self.testapp.post('/delete_test_data', {
        'pattern': '*/*/SunSpider',
    })
    self.ExecuteTaskQueueTasks(
        '/delete_test_data', delete_test_data._TASK_QUEUE_NAME)
    self._AssertNotExists([
        'ChromiumPerf/mac/SunSpider/Total/t',
        'ChromiumPerf/win7/SunSpider/Total/t',
        'ChromiumPerf/mac/SunSpider/Total/t_ref',
        'ChromiumPerf/win7/SunSpider/Total/t_ref',
        'ChromiumPerf/mac/SunSpider/Total/t_extwr',
        'ChromiumPerf/win7/SunSpider/Total/t_extwr',
        'ChromiumPerf/mac/SunSpider/Total',
        'ChromiumPerf/win7/SunSpider/Total',
        'ChromiumPerf/mac/SunSpider/3d-cube/t',
        'ChromiumPerf/win7/SunSpider/3d-cube/t',
        'ChromiumPerf/mac/SunSpider',
        'ChromiumPerf/win7/SunSpider',
    ])
    self._AssertExists([
        'ChromiumPerf/win7/moz',
        'ChromiumPerf/mac/moz',
        'ChromiumPerf/win7/moz/read_op_b/r_op_b',
        'ChromiumPerf/mac/moz/read_op_b/r_op_b',
    ])


if __name__ == '__main__':
  unittest.main()
