# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2023 Horizon Robotics Co,. Ltd

import pytest

@pytest.mark.buildconfigspec('cmd_pci')
def test_pci_list(u_boot_console):
    response = u_boot_console.run_command('pci')
    bdf_index = response.find('BusDevFun')
    assert bdf_index != -1
    bdfs = (line[bdf_index:].split()[0]
               for line in response[:-1].split('\n')[2:])

    for bdf in bdfs:
        response = u_boot_console.run_command('pci header %s' % str(bdf))
        for id in response.split('\n')[0:2]:
            ID=int(id.split()[-1], 16)
            assert ID != 0
