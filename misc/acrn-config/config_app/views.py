# Copyright (C) 2019 Intel Corporation.
# SPDX-License-Identifier: BSD-3-Clause

"""View pages for config app.

"""

import os
from datetime import datetime
from shutil import copyfile

# flask: Copyright 2010 Pallets
# SPDX-License-Identifier: BSD-3-Clause
# Refer to https://github.com/pallets/flask/blob/master/LICENSE.rst for the permission notice.
from flask import request, render_template, Blueprint, redirect, url_for, current_app

# werkzeug: Copyright 2007 Pallets
# SPDX-License-Identifier: BSD-3-Clause
# Refer to https://github.com/pallets/werkzeug/blob/master/LICENSE.rst for the permission notice.
from werkzeug.utils import secure_filename

from controller import XmlConfig
from scenario_config.scenario_cfg_gen import get_scenario_item_values
from scenario_config.scenario_cfg_gen import validate_scenario_setting
from launch_config.launch_cfg_gen import get_launch_item_values
from launch_config.launch_cfg_gen import validate_launch_setting


CONFIG_APP = Blueprint('CONFIG_APP', __name__, template_folder='templates')


@CONFIG_APP.route('/')
def index():
    """
    render the index page
    :return: the render template of index page
    """
    return redirect(url_for('CONFIG_APP.scenarios'))


@CONFIG_APP.route('/scenario', methods=['GET'])
def scenarios():
    """
    render the scenario parent setting page
    :return: the render template of scenario setting parent page
    """
    board_info, board_type, scenario_config, launch_config = get_xml_configs()
    print(board_info, scenario_config, launch_config)
    (bios_info, base_board_info) = get_board_info(board_info)

    return render_template('scenario.html', board_info_list=get_board_list(),
                           board_info=board_info, board_type=board_type,
                           bios_info=bios_info, base_board_info=base_board_info,
                           scenarios=scenario_config.list_all(xml_type='scenario'),
                           launches=launch_config.list_all(xml_type='uos_launcher'),
                           scenario='', root=None)


@CONFIG_APP.route('/scenario/<scenario_name>', methods=['GET'])
def scenario(scenario_name):
    """
    render the specified scenario setting page
    :param scenario_name: the scenario type
    :return: the render template of the specified scenario setting page
    """

    board_info, board_type, scenario_config, launch_config = get_xml_configs()
    print(board_info, scenario_config, launch_config)
    (bios_info, base_board_info) = get_board_info(board_info)

    current_app.config.update(SCENARIO=scenario_name)

    scenario_config.set_curr(scenario_name)

    scenario_item_values = {}
    if board_info is not None and board_type is not None:
        scenario_file_path = os.path.join(current_app.config.get('CONFIG_PATH'), board_type,
                                          'user_defined', scenario_name + '.xml')
        if os.path.isfile(scenario_file_path):
            scenario_item_values = get_scenario_item_values(
                os.path.join(os.path.dirname(os.path.abspath(__file__)), 'res', board_info+'.xml'),
                scenario_file_path)

    print('scenario_item_values: ', scenario_item_values)

    return render_template('scenario.html', board_info_list=get_board_list(),
                           board_info=board_info, board_type=board_type,
                           bios_info=bios_info, base_board_info=base_board_info,
                           scenarios=scenario_config.list_all(xml_type='scenario'),
                           launches=launch_config.list_all(xml_type='uos_launcher'),
                           scenario=scenario_name, root=scenario_config.get_curr_root(),
                           scenario_item_values=scenario_item_values)


@CONFIG_APP.route('/launch', methods=['GET'])
def launches():
    """
    render the parent launch setting page
    :return: the render template of launch setting page
    """
    board_info, board_type, scenario_config, launch_config = get_xml_configs()
    print(board_info, scenario_config, launch_config)
    (bios_info, base_board_info) = get_board_info(board_info)

    return render_template('launch.html', board_info_list=get_board_list(),
                           board_info=board_info, board_type=board_type,
                           bios_info=bios_info, base_board_info=base_board_info,
                           scenarios=scenario_config.list_all(xml_type='scenario'),
                           launches=launch_config.list_all(xml_type='uos_launcher'),
                           launch='', root=None)


@CONFIG_APP.route('/launch/<launch_name>', methods=['GET'])
def launch(launch_name):
    """
    render the specified launch setting page
    :param launch_name: the launch type
    :return: the render template of specified launch setting page
    """
    board_info, board_type, scenario_config, launch_config = get_xml_configs()
    print(board_info, scenario_config, launch_config)
    (bios_info, base_board_info) = get_board_info(board_info)

    launch_config.set_curr(launch_name)

    launch_item_values = {}
    if board_info is not None:
        launch_item_values = get_launch_item_values(
            os.path.join(os.path.dirname(os.path.abspath(__file__)), 'res', board_info + '.xml'))

    print('launch_item_values: ', launch_item_values)

    return render_template('launch.html', board_info_list=get_board_list(),
                           board_info=board_info, board_type=board_type,
                           bios_info=bios_info, base_board_info=base_board_info,
                           scenarios=scenario_config.list_all(xml_type='scenario'),
                           launches=launch_config.list_all(xml_type='uos_launcher'),
                           launch=launch_name, root=launch_config.get_curr_root(),
                           scenario=current_app.config.get('SCENARIO'),
                           launch_item_values=launch_item_values)


@CONFIG_APP.route('/save_scenario', methods=['POST'])
def save_scenario():
    """
    save scenario setting.
    :return: the error list for the edited scenario setting.
    """
    scenario_config_data = request.json if request.method == "POST" else request.args

    xml_configs = get_xml_configs()
    board_type = xml_configs[1]
    scenario_config = xml_configs[3]

    if board_type is None or xml_configs[0] is None:
        return {'status': 'fail',
                'error_list': {'error': 'Please select the board info before this operation.'}}

    scenario_path = os.path.join(current_app.config.get('CONFIG_PATH'), board_type)
    old_scenario_name = scenario_config_data['old_scenario_name']
    scenario_config.set_curr(old_scenario_name)
    for key in scenario_config_data:
        if key not in ['old_scenario_name', 'new_scenario_name', 'generator', 'add_vm_type']:
            if isinstance(scenario_config_data[key], list):
                scenario_config.set_curr_list(scenario_config_data[key], *tuple(key.split(',')))
            else:
                scenario_config.set_curr_value(scenario_config_data[key], *tuple(key.split(',')))

    generator = scenario_config_data['generator']
    if generator is not None:
        if generator == 'remove_vm_kata':
            scenario_config.delete_curr_key('vm:desc=specific for Kata')
        elif generator == 'add_vm_kata':
            # clone vm kata from generic config
            generic_scenario_config = get_generic_scenario_config(scenario_config)
            generic_scenario_config_root = generic_scenario_config.get_curr_root()
            elem_kata = None
            for vm in generic_scenario_config_root.getchildren():
                if 'desc' in vm.attrib and vm.attrib['desc'] == 'specific for Kata':
                    elem_kata = vm
                    break
            if elem_kata is not None:
                scenario_config.clone_curr_elem(elem_kata)
        elif generator.startswith('add_vm:'):
            vm_list = []
            for vm in scenario_config.get_curr_root().getchildren():
                if vm.tag == 'vm':
                    vm_list.append(vm.attrib['id'])
            if len(vm_list) >= 8:
                return {'status': 'fail',
                        'error_list': {'error': 'Can not add a new VM. Max VM number is 8.'}}
            curr_vm_id = generator.split(':')[1]
            add_vm_type = scenario_config_data['add_vm_type']
            generic_scenario_config = get_generic_scenario_config(scenario_config, add_vm_type)
            generic_scenario_config_root = generic_scenario_config.get_curr_root()
            vm_to_add = []
            if str(curr_vm_id) == '-1':
                curr_vm_index = 1
            else:
                curr_vm_index = len(vm_list) + 1
                for i in range(len(vm_list)):
                    if curr_vm_id == vm_list[i]:
                        curr_vm_index = i + 2
                        break
            for vm in generic_scenario_config_root.getchildren():
                if vm.tag == 'vm':
                    for i in range(0, 7):
                        if str(i) not in vm_list:
                            break
                    vm.attrib['id'] = str(i)
                    vm_to_add.append(vm)
            for vm in vm_to_add:
                scenario_config.insert_curr_elem(curr_vm_index, vm)
                curr_vm_index += 1
        elif generator.startswith('remove_vm:'):
            remove_vm_id = generator.split(':')[1]
            scenario_config.delete_curr_key('vm:id='+remove_vm_id.strip())

    tmp_scenario_file = os.path.join(scenario_path, 'user_defined',
                                     'tmp_'+scenario_config_data['new_scenario_name']+'.xml')
    # if os.path.isfile(tmp_scenario_file):
    #     os.remove(tmp_scenario_file)
    scenario_config.save('tmp_'+scenario_config_data['new_scenario_name'])

    # call validate function
    error_list = {}
    new_scenario_name = scenario_config_data['new_scenario_name']
    rename = False
    try:
        if generator is None or not (generator.startswith('add_vm:') or generator.startswith('remove_vm:')):
            (error_list, vm_info) = validate_scenario_setting(
                os.path.join(os.path.dirname(os.path.abspath(__file__)), 'res', xml_configs[0]+'.xml'),
                tmp_scenario_file)
            print('vm_info: ', vm_info)
    except Exception as error:
        if os.path.isfile(tmp_scenario_file):
            os.remove(tmp_scenario_file)
        return {'status': 'fail', 'file_name': new_scenario_name,
                'rename': rename, 'error_list': {'error': str(error)}}

    if not error_list:
        scenario_config.save(new_scenario_name)
        if old_scenario_name != new_scenario_name:
            os.remove(os.path.join(scenario_path, 'user_defined', old_scenario_name + '.xml'))

    if os.path.isfile(tmp_scenario_file):
        os.remove(tmp_scenario_file)

    return {'status': 'success', 'file_name': new_scenario_name,
            'rename': rename, 'error_list': error_list}


@CONFIG_APP.route('/save_launch', methods=['POST'])
def save_launch():
    """
    save launch setting.
    :return: the error list of the edited launch setting.
    """
    launch_config_data = request.json if request.method == "POST" else request.args
    print(launch_config_data)
    xml_configs = get_xml_configs()
    launch_config = xml_configs[3]

    if xml_configs[1] is None or xml_configs[0] is None:
        return {'status': 'fail',
                'error_list': {'error': 'Please select the board info before this operation.'}}

    old_launch_name = launch_config_data['old_launch_name']
    launch_config.set_curr(old_launch_name)

    for key in launch_config_data:
        if key not in ['old_launch_name', 'new_launch_name', 'generator', 'add_launch_type', 'scenario_name']:
            if isinstance(launch_config_data[key], list):
                launch_config.set_curr_list(launch_config_data[key], *tuple(key.split(',')))
            else:
                launch_config.set_curr_value(launch_config_data[key], *tuple(key.split(',')))

    generator = launch_config_data['generator']
    if generator is not None:
        if generator.startswith('add_vm:'):
            vm_list = []
            for vm in launch_config.get_curr_root().getchildren():
                if vm.tag == 'uos':
                    vm_list.append(vm.attrib['id'])
            if len(vm_list) >= 8:
                return {'status': 'fail',
                        'error_list': {'error': 'Can not add a new VM. Max VM number is 8.'}}
            curr_vm_id = generator.split(':')[1].strip()
            add_launch_type = launch_config_data['add_launch_type']
            generic_scenario_config = get_generic_scenario_config(launch_config, add_launch_type)
            generic_scenario_config_root = generic_scenario_config.get_curr_root()
            vm_to_add = []
            if str(curr_vm_id) == '-1':
                curr_vm_index = len(vm_list)
            else:
                curr_vm_index = 0
                for i in range(len(vm_list)):
                    if curr_vm_id == vm_list[i]:
                        curr_vm_index = i + 1
                        break
            for vm in generic_scenario_config_root.getchildren():
                if vm.tag == 'uos':
                    for i in range(1, 8):
                        if str(i) not in vm_list:
                            break
                    vm.attrib['id'] = str(i)
                    vm_to_add.append(vm)
            # print('-'*100)
            # print(generator)
            # print(vm_list)
            # print(curr_vm_id)
            # print(curr_vm_index)
            # print(i)
            for vm in vm_to_add:
                launch_config.insert_curr_elem(curr_vm_index, vm)
        elif generator.startswith('remove_vm:'):
            remove_vm_id = generator.split(':')[1]
            launch_config.delete_curr_key('uos:id='+remove_vm_id.strip())

    scenario_name = launch_config_data['scenario_name']
    scenario_file_path = os.path.join(current_app.config.get('CONFIG_PATH'),
                                      current_app.config.get('BOARD_TYPE'),
                                      'user_defined', scenario_name + '.xml')
    launch_config.set_curr_attr('scenario', scenario_name)

    tmp_launch_file = os.path.join(current_app.config.get('CONFIG_PATH'), xml_configs[1],
                                   'user_defined',
                                   'tmp_' + launch_config_data['new_launch_name'] + '.xml')
    # if os.path.isfile(tmp_launch_file):
    #     os.remove(tmp_launch_file)
    launch_config.save('tmp_' + launch_config_data['new_launch_name'])

    # call validate function
    error_list = {}
    rename = False
    try:
        if generator is None or not (generator.startswith('add_vm:') or generator.startswith('remove_vm:')):
            (error_list, pthru_sel, virtio, dm_value) = validate_launch_setting(
                os.path.join(os.path.dirname(os.path.abspath(__file__)), 'res', xml_configs[0]+'.xml'),
                scenario_file_path,
                tmp_launch_file)
            print(pthru_sel, virtio, dm_value)
    except Exception as error:
        if os.path.isfile(tmp_launch_file):
            os.remove(tmp_launch_file)
        return {'status': 'fail', 'file_name': launch_config_data['new_launch_name'],
                'rename': rename, 'error_list': {'launch config error': str(error)}}

    if not error_list:
        launch_config.save(launch_config_data['new_launch_name'])
        if old_launch_name != launch_config_data['new_launch_name']:
            os.remove(os.path.join(current_app.config.get('CONFIG_PATH'), xml_configs[1], old_launch_name + '.xml'))

    if os.path.isfile(tmp_launch_file):
        os.remove(tmp_launch_file)

    return {'status': 'success', 'file_name': launch_config_data['new_launch_name'],
            'rename': rename, 'error_list': error_list}


@CONFIG_APP.route('/check_setting_exist', methods=['POST'])
def check_setting_exist():
    """
    check the setting exist or not.
    :return: setting exist or not.
    """
    config_data = request.json if request.method == "POST" else request.args

    board_info = current_app.config.get('BOARD_TYPE')
    setting_path = os.path.join(current_app.config.get('CONFIG_PATH'), board_info)
    if 'old_scenario_name' in list(config_data.keys()) and 'new_scenario_name' in list(config_data.keys()):
        if config_data['old_scenario_name'] != config_data['new_scenario_name'] and \
           os.path.isfile(os.path.join(setting_path, 'user_defined', config_data['new_scenario_name'] + '.xml')):
            return {'exist': 'yes'}
        else:
            return {'exist': 'no'}
    elif 'old_launch_name' in list(config_data.keys()) and 'new_launch_name' in list(config_data.keys()):
        if config_data['old_launch_name'] != config_data['new_launch_name'] and \
           os.path.isfile(os.path.join(setting_path, 'user_defined', config_data['new_launch_name'] + '.xml')):
            return {'exist': 'yes'}
        else:
            return {'exist': 'no'}
    elif 'create_name' in list(config_data.keys()):
        if os.path.isfile(os.path.join(setting_path, 'user_defined',  config_data['create_name'] + '.xml')):
            return {'exist': 'yes'}
        else:
            return {'exist': 'no'}
    else:
        return {'exist': 'no'}


@CONFIG_APP.route('/create_setting', methods=['POST'])
def create_setting():
    """
    create a new scenario or launch setting.
    :return: the status and error list of the created scenario or launch setting.
    """
    create_config_data = request.json if request.method == "POST" else request.args
    mode = create_config_data['mode']
    default_name = create_config_data['default_name']
    create_name = create_config_data['create_name']

    xml_configs = get_xml_configs(True)
    board_type = xml_configs[1]
    scenario_config = xml_configs[2]
    launch_config = xml_configs[3]

    if board_type is None or xml_configs[0] is None:
        return {'status': 'fail',
                'error_list': {'error': 'Please select the board info before this operation.'}}

    setting_path = os.path.join(current_app.config.get('CONFIG_PATH'), board_type, 'user_defined')
    if not os.path.isdir(setting_path):
        os.makedirs(setting_path)

    if create_config_data['type'] == 'launch':
        launch_file = os.path.join(setting_path,  create_name + '.xml')
        if os.path.isfile(launch_file):
            os.remove(launch_file)

        if mode == 'create':
            template_file_name = 'LAUNCH_STANDARD_VM'
            src_file_name = os.path.join(current_app.config.get('CONFIG_PATH'), 'template', template_file_name+'.xml')
        else:
            src_file_name = os.path.join(current_app.config.get('CONFIG_PATH'), board_type, default_name + '.xml')
        copyfile(src_file_name,
                 os.path.join(current_app.config.get('CONFIG_PATH'), board_type, 'user_defined', create_name + '.xml'))

        launch_config.set_curr(create_name)
        if mode == 'create':
            launch_config.delete_curr_key('uos:id=1')
        launch_config.save(create_name)
        return {'status': 'success', 'setting': create_name, 'error_list': {}}

    elif create_config_data['type'] == 'scenario':
        scenario_file = os.path.join(setting_path, create_name + '.xml')
        if os.path.isfile(scenario_file):
            os.remove(scenario_file)

        if mode == 'create':
            template_file_name = 'HV'
            src_file_name = os.path.join(current_app.config.get('CONFIG_PATH'), 'template', template_file_name+'.xml')
        else:
            src_file_name = os.path.join(current_app.config.get('CONFIG_PATH'), board_type, default_name + '.xml')
        copyfile(src_file_name,
                 os.path.join(current_app.config.get('CONFIG_PATH'), board_type, 'user_defined',
                              create_name + '.xml'))

        scenario_config.save(create_name)
        return {'status': 'success', 'setting': create_name, 'error_list': {}}

    else:
        return {'status': 'fail', 'error_list': {'name': 'Unsupported setting type. '}}


@CONFIG_APP.route('/remove_setting', methods=['POST'])
def remove_setting():
    """
    remove current setting from config app
    :return: the return message
    """
    remove_config_data = request.json if request.method == "POST" else request.args

    old_setting_name = remove_config_data['old_setting_name']

    if current_app.config.get('BOARD_TYPE') is None:
        return {'status': 'Board info not set before remove current setting'}

    old_setting_path = os.path.join(current_app.config.get('CONFIG_PATH'),
                                    current_app.config.get('BOARD_TYPE'),
                                    'user_defined',
                                    old_setting_name + '.xml')

    if os.path.isfile(old_setting_path):
        os.remove(old_setting_path)
    return {'status': 'success'}


@CONFIG_APP.route('/generate_src', methods=['POST'])
def generate_src():
    """
    generate board src or scenario src
    :return: the error message
    """
    generator_config_data = request.json if request.method == "POST" else request.args

    src_type = generator_config_data['type']
    board_info = generator_config_data['board_info']
    board_type = current_app.config.get('BOARD_TYPE')
    board_info_xml = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                  'res', board_info+'.xml')
    scenario_setting = generator_config_data['scenario_setting']
    scenario_setting_xml = os.path.join(current_app.config.get('CONFIG_PATH'), board_type,
                                        'user_defined', scenario_setting+'.xml')
    src_path = generator_config_data['src_path']
    print(src_path)
    launch_setting_xml = None
    if 'launch_setting' in generator_config_data:
        launch_setting = generator_config_data['launch_setting']
        launch_setting_xml = os.path.join(current_app.config.get('CONFIG_PATH'),
                                          board_type, 'user_defined', launch_setting + '.xml')
    msg = {}
    error_list = {}
    status = 'success'
    if src_type == 'generate_config_src':
        try:
            from board_config.board_cfg_gen import ui_entry_api
            error_list = ui_entry_api(board_info_xml, scenario_setting_xml, src_path)
        except Exception as error:
            status = 'fail'
            error_list = {'board setting error': str(error)}

        try:
            from scenario_config.scenario_cfg_gen import ui_entry_api
            error_list = ui_entry_api(board_info_xml, scenario_setting_xml, src_path)
        except Exception as error:
            status = 'fail'
            error_list = {'scenario setting error': str(error)}
    elif src_type == 'generate_launch_script':
        if scenario_setting.startswith('user_defined_'):
            scenario_setting = scenario_setting[13:]
            scenario_setting_xml = os.path.join(current_app.config.get('CONFIG_PATH'), board_type,
                                                'user_defined', scenario_setting + '.xml')
        else:
            scenario_setting_xml = os.path.join(current_app.config.get('CONFIG_PATH'), board_type,
                                                scenario_setting + '.xml')

        try:
            from launch_config.launch_cfg_gen import ui_entry_api
            error_list = ui_entry_api(board_info_xml, scenario_setting_xml, launch_setting_xml, src_path)
        except Exception as error:
            status = 'fail'
            error_list = {'launch setting error': str(error)}
    else:
        status = 'fail'
        error_list = {'error': 'generator type not specified'}

    msg = {'status': status, 'error_list': error_list}
    return msg


@CONFIG_APP.route('/upload_board_info', methods=['POST'])
def upload_board_info():
    """
    upload board info xml file
    :return: the upload status
    """
    if request.method == 'POST':
        if 'file' not in request.files:
            return {'status': 'Error: no file uploaded'}
        file = request.files['file']
        if file and '.' in file.filename and file.filename.rsplit('.', 1)[1] in ['xml']:
            filename = secure_filename(file.filename)
            tmp_filename = 'tmp_' + filename
            save_tmp_board_path = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                               'res', tmp_filename)
            file.save(save_tmp_board_path)

            board_type_list = []
            config_path = current_app.config.get('CONFIG_PATH')
            for config_name in os.listdir(config_path):
                if os.path.isdir(os.path.join(config_path, config_name)) \
                        and config_name != 'generic':
                    board_type_list.append(config_name)

            res_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'res')
            if not os.path.isdir(res_path):
                os.makedirs(res_path)

            board_info_config = XmlConfig(res_path)
            board_info_config.set_curr(tmp_filename.rsplit('.', 1)[0])
            board_info_root = board_info_config.get_curr_root()
            board_type = None
            if board_info_root and 'board' in board_info_root.attrib \
                and 'scenario' not in board_info_root.attrib \
                    and 'uos_launcher' not in board_info_root.attrib:
                board_type = board_info_root.attrib['board']
            if not board_type:
                os.remove(save_tmp_board_path)
                return {'status': 'Error on parsing Board info\n'
                                  'check the xml syntax and whether there is only the board '
                                  'attribute in the board info file'}

            os.rename(save_tmp_board_path,
                      os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                   'res', filename))
            info = 'updated'
            if board_type not in board_type_list:
                info = board_type
                os.makedirs(os.path.join(config_path, board_type))
                for generic_name in os.listdir(os.path.join(config_path, 'generic')):
                    generic_file = os.path.join(config_path, 'generic', generic_name)
                    if os.path.isfile(generic_file):
                        new_file = os.path.join(config_path, board_type, generic_name)
                        copyfile(generic_file, new_file)
                        xml_config = XmlConfig(os.path.join(current_app.config.get('CONFIG_PATH'),
                                                            board_type))
                        xml_config.set_curr(generic_name[:-4])
                        xml_config.set_curr_attr('board', board_type)
                        xml_config.save(generic_name[:-4], user_defined=False)

            board_info = os.path.splitext(file.filename)[0]
            current_app.config.update(BOARD_INFO=board_info)
            current_app.config.update(BOARD_TYPE=board_type)

            return {'status': 'success', 'info': info}

    return {'status': 'Error: upload failed'}


@CONFIG_APP.route('/select_board', methods=['POST'])
def select_board():
    """
    select one board info
    :return: the selected board info
    """
    data = request.json if request.method == "POST" else request.args
    board_info = data['board_info']
    current_app.config.update(BOARD_INFO=board_info)

    board_type = get_board_type(board_info)
    current_app.config.update(BOARD_TYPE=board_type)
    if board_type:
        return board_type

    return ''


@CONFIG_APP.route('/upload_scenario', methods=['POST'])
def upload_scenario():
    """
    upload scenario setting xml file
    :return: the upload status
    """
    if request.method == 'POST':
        if 'file' not in request.files:
            return {'status': 'no file uploaded'}
        file = request.files['file']
        if file and '.' in file.filename and file.filename.rsplit('.', 1)[1] in ['xml']:
            filename = secure_filename(file.filename)
            print(filename)
            scenario_file_name = os.path.splitext(file.filename)[0]
            board_type = current_app.config.get('BOARD_TYPE')

            tmp_scenario_name = 'tmp_' + scenario_file_name + '.xml'
            tmp_scenario_file = os.path.join(current_app.config.get('CONFIG_PATH'), board_type,
                                             'user_defined', tmp_scenario_name)
            tmp_scenario_folder = os.path.dirname(tmp_scenario_file)
            if not os.path.exists(tmp_scenario_folder):
                os.makedirs(tmp_scenario_folder)
            if os.path.isfile(tmp_scenario_file):
                os.remove(tmp_scenario_file)

            file.save(tmp_scenario_file)

            tmp_xml_config = XmlConfig(os.path.join(current_app.config.get('CONFIG_PATH'),
                                                    board_type, 'user_defined'))
            tmp_xml_config.set_curr(tmp_scenario_name[:-4])
            status = None
            if tmp_xml_config.get_curr_root() is None:
                status = 'Error on parsing the scenario xml file, \n' \
                         'check the xml syntax and config items.'
            else:
                tmp_root = tmp_xml_config.get_curr_root()
                if 'board' not in tmp_root.attrib or 'scenario' not in tmp_root.attrib \
                        or 'uos_launcher' in tmp_root.attrib:
                    status = 'Invalid scenario xml file, \nonly board and scenario ' \
                             'need to be configured.'
                elif tmp_root.attrib['board'] != current_app.config.get('BOARD_TYPE'):
                    status = 'Current board: {} mismatched with the board in the scenario file,' \
                             '\nplease reselect or upload the board info: {}'\
                        .format(current_app.config.get('BOARD_TYPE'), tmp_root.attrib['board'])
            if status is not None:
                if os.path.isfile(tmp_scenario_file):
                    os.remove(tmp_scenario_file)
                return {'status': status}

            error_list = {}
            new_scenario_name = scenario_file_name
            rename = False
            if not error_list:
                new_scenario_path = os.path.join(current_app.config.get('CONFIG_PATH'), board_type,
                                                 'user_defined', scenario_file_name + '.xml')
                if os.path.isfile(new_scenario_path):
                    new_scenario_name = new_scenario_name + '_' \
                                        + datetime.now().strftime('%Y%m%d%H%M%S')
                    rename = True

            os.rename(tmp_scenario_file, os.path.join(current_app.config.get('CONFIG_PATH'),
                                                      board_type, 'user_defined',
                                                      new_scenario_name + '.xml'))

            return {'status': 'success', 'file_name': new_scenario_name,
                    'rename': rename, 'error_list': error_list}

    return {'status': 'unsupported method'}


@CONFIG_APP.route('/upload_launch', methods=['POST'])
def upload_launch():
    """
    upload scenario setting xml file
    :return: the upload status
    """
    if request.method == 'POST':
        if 'file' not in request.files:
            return {'status': 'no file uploaded'}
        file = request.files['file']
        if file and '.' in file.filename and file.filename.rsplit('.', 1)[1] in ['xml']:
            filename = secure_filename(file.filename)
            print(filename)
            launch_file_name = os.path.splitext(file.filename)[0]
            board_type = current_app.config.get('BOARD_TYPE')

            tmp_launch_name = 'tmp_' + launch_file_name + '.xml'
            tmp_launch_file = os.path.join(current_app.config.get('CONFIG_PATH'), board_type,
                                           'user_defined', tmp_launch_name)
            if os.path.isfile(tmp_launch_file):
                os.remove(tmp_launch_file)

            file.save(tmp_launch_file)

            tmp_xml_config = XmlConfig(os.path.join(current_app.config.get('CONFIG_PATH'),
                                                    board_type, 'user_defined'))
            tmp_xml_config.set_curr(tmp_launch_name[:-4])
            status = None
            if tmp_xml_config.get_curr_root() is None:
                status = 'Error on parsing the scenario xml file, \n' \
                         'check the xml syntax and config items.'
            else:
                tmp_root = tmp_xml_config.get_curr_root()
                if 'board' not in tmp_root.attrib or 'scenario' not in tmp_root.attrib \
                        or 'uos_launcher' not in tmp_root.attrib:
                    status = 'Invalid launch xml file, \nboard, scenario,' \
                             'and uos_launcher need to be configured.'
                elif tmp_root.attrib['board'] != current_app.config.get('BOARD_TYPE'):
                    status = 'Current board: {} mismatched with the board in the launch file,' \
                             '\nplease reselect or upload the board info: {}' \
                        .format(current_app.config.get('BOARD_TYPE'), tmp_root.attrib['board'])
            if status is not None:
                if os.path.isfile(tmp_launch_file):
                    os.remove(tmp_launch_file)
                return {'status': status}

            error_list = {}
            new_launch_name = launch_file_name
            rename = False
            if not error_list:
                new_launch_path = os.path.join(current_app.config.get('CONFIG_PATH'), board_type,
                                               'user_defined', launch_file_name + '.xml')
                if os.path.isfile(new_launch_path):
                    new_launch_name = new_launch_name + '_' + \
                                      datetime.now().strftime('%Y%m%d%H%M%S')
                    rename = True

            os.rename(tmp_launch_file, os.path.join(current_app.config.get('CONFIG_PATH'),
                                                    board_type, 'user_defined',
                                                    new_launch_name + '.xml'))

            return {'status': 'success', 'file_name': new_launch_name,
                    'rename': rename, 'error_list': error_list}

    return {'status': 'unsupported method'}


def get_board_list():
    """
    get all available board info files
    :return: the file list of board info
    """
    res_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'res')
    if not os.path.isdir(res_path):
        os.makedirs(res_path)
    board_info_list = []
    for file in os.listdir(res_path):
        if os.path.isfile(os.path.join(res_path, file)) and \
                '.' in file and file.rsplit('.', 1)[1] in ['xml']:
            board_info_list.append(file.rsplit('.', 1)[0])
    return board_info_list


def get_xml_configs(user_defined=True):
    """
    get xml config related variables
    :return: board_info, board_config, scenario_config, launch_config
    """

    config_path = None
    board_info = current_app.config.get('BOARD_INFO')
    board_type = get_board_type(board_info)
    if board_type is not None:
        config_path = os.path.join(current_app.config.get('CONFIG_PATH'), board_type)

    scenario_config = XmlConfig(config_path, not user_defined)
    launch_config = XmlConfig(config_path, not user_defined)

    return board_info, board_type, scenario_config, launch_config


def get_generic_scenario_config(scenario_config, add_vm_type=None):

    if add_vm_type is not None:
        config_path = os.path.join(current_app.config.get('CONFIG_PATH'), 'template')
        generic_scenario_config = XmlConfig(config_path)
        if os.path.isfile(os.path.join(config_path, add_vm_type + '.xml')):
            generic_scenario_config.set_curr(add_vm_type)
            return generic_scenario_config
        else:
            return None
    config_path = os.path.join(current_app.config.get('CONFIG_PATH'), 'generic')
    generic_scenario_config = XmlConfig(config_path)
    for file in os.listdir(config_path):
        if os.path.isfile(os.path.join(config_path, file)) and \
                os.path.splitext(file)[1] == '.xml':
            generic_scenario_config.set_curr(os.path.splitext(file)[0])
            generic_scenario_config_root = generic_scenario_config.get_curr_root()
            if 'scenario' in generic_scenario_config_root.attrib \
                and 'uos_launcher' not in generic_scenario_config_root.attrib \
                and generic_scenario_config_root.attrib['scenario'] == \
                    scenario_config.get_curr_root().attrib['scenario']:
                return generic_scenario_config

    return None


def get_board_config(board_info):
    """
    get board config
    :param board_info: the board info file
    :return: the board type
    """
    board_config = None
    if board_info is not None:
        board_config = XmlConfig(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'res'))
        board_config.set_curr(board_info)

    return board_config


def get_board_type(board_info):
    """
    get board info type
    :param board_info: the board info file
    :return: the board type
    """
    board_config = get_board_config(board_info)
    board_type = None
    if board_config is not None:
        board_info_root = board_config.get_curr_root()
        if board_info_root and 'board' in board_info_root.attrib:
            board_type = board_info_root.attrib['board']

    return board_type


def get_board_info(board_info):
    """
    get board info type
    :param board_info: the board info file
    :return: the board type
    """
    board_config = get_board_config(board_info)
    bios_info = None
    base_board_info = None
    if board_config is not None:
        board_info_root = board_config.get_curr_root()
        if board_info_root:
            for item in board_info_root.getchildren():
                if item.tag == 'BIOS_INFO':
                    for line in item.text.split('\n'):
                        if line.strip() != '':
                            if bios_info is None:
                                bios_info = line.strip()
                            else:
                                bios_info += ('\n' + line.strip())
                elif item.tag == 'BASE_BOARD_INFO':
                    for line in item.text.split('\n'):
                        if line.strip() != '':
                            if base_board_info is None:
                                base_board_info = line.strip()
                            else:
                                base_board_info += ('\n' + line.strip())

    return (bios_info, base_board_info)