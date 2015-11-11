#!/usr/bin/env python
#
from opentuner import ConfigurationManipulator
from opentuner.search.manipulator import PowerOfTwoParameter
from opentuner.search.manipulator import IntegerParameter
mdriver_manipulator = ConfigurationManipulator()

"""
See opentuner/search/manipulator.py for more parameter types,
like IntegerParameter, EnumParameter, etc.

TODO(project3): Implement the parameters of your allocator. Once
you have at least one other parameters, feel free to remove ALIGNMENT.
"""
#mdriver_manipulator.add_parameter(IntegerParameter('SMALL_BIN_SEARCH_MAX', 0, 32))
mdriver_manipulator.add_parameter(IntegerParameter('LARGE_BIN_SEARCH_MAX', 0, 32))
mdriver_manipulator.add_parameter(IntegerParameter('EXTENSION_SIZE', 8, 2000))
#mdriver_manipulator.add_parameter(IntegerParameter('INITIAL_CHUNK_SIZE', 0, 40000))

