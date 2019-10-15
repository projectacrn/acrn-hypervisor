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

    return render_template('scenario.html', board_info_list=get_board_list(),
                           board_info=board_info, board_type=board_type,
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

    board_info, board_type, scenario_config, launch_config = \
        get_xml_configs(scenario_name.startswith('user_defined_'))
    print(board_info, scenario_config, launch_config)
    current_app.config.update(SCENARIO=scenario_name)

    if scenario_name.startswith('user_defined_'):
        scenario_config.set_curr(scenario_name[13:])
    else:
        scenario_config.set_curr(scenario_name)

    scenario_item_values = {}
    if board_info is not None and board_type is not None:
        scenario_file_path = os.path.join(current_app.config.get('CONFIG_PATH'),
                                          board_type, scenario_name + '.xml')
        if scenario_name.startswith('user_defined_'):
            scenario_file_path = os.path.join(current_app.config.get('CONFIG_PATH'), board_type,
                                              'user_defined', scenario_name[13:] + '.xml')
        if os.path.isfile(scenario_file_path):
            scenario_item_values = get_scenario_item_values(
                os.path.join(os.path.dirname(os.path.abspath(__file__)), 'res', board_info+'.xml'),
                scenario_file_path)

    print('scenario_item_values: ', scenario_item_values)

    return render_template('scenario.html', board_info_list=get_board_list(),
                           board_info=board_info, board_type=board_type,
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

    return render_template('launch.html', board_info_list=get_board_list(),
                           board_info=board_info, board_type=board_type,
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
    print('launch: ', launch_name)
    board_info, board_type, scenario_config, launch_config = \
        get_xml_configs(launch_name.startswith('user_defined_'))
    print(board_info, scenario_config, launch_config)

    if launch_name.startswith('user_defined_'):
        launch_config.set_curr(launch_name[13:])
    else:
        launch_config.set_curr(launch_name)

    launch_item_values = {}
    if board_info is not None:
        launch_item_values = get_launch_item_values(
            os.path.join(os.path.dirname(os.path.abspath(__file__)), 'res', board_info + '.xml'))

    print('launch_item_values: ', launch_item_values)

    return render_template('launch.html', board_info_list=get_board_list(),
                           board_info=board_info, board_type=board_type,
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
    print("save_scenario")
    print(scenario_config_data)

    xml_configs = \
        get_xml_configs(scenario_config_data['old_scenario_name'].startswith('user_defined_'))
    board_type = xml_configs[1]
    scenario_config = xml_configs[3]

    if board_type is None or xml_configs[0] is None:
        return {'status': 'fail',
                'error_list': {'error': 'Please select the board info before this operation.'}}

    scenario_path = os.path.join(current_app.config.get('CONFIG_PATH'), board_type)
    old_scenario_name = scenario_config_data['old_scenario_name']
    if scenario_config_data['old_scenario_name'].startswith('user_defined_'):
        old_scenario_name = scenario_config_data['old_scenario_name'][13:]
    scenario_config.set_curr(old_scenario_name)
    for key in scenario_config_data:
        if key not in ['old_scenario_name', 'new_scenario_name', 'board_info_file',
                       'board_info_upload']:
            if isinstance(scenario_config_data[key], list):
                scenario_config.set_curr_list(scenario_config_data[key], *tuple(key.split(',')))
            else:
                scenario_config.set_curr_value(scenario_config_data[key], *tuple(key.split(',')))

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
        (error_list, vm_info) = validate_scenario_setting(
            os.path.join(os.path.dirname(os.path.abspath(__file__)), 'res', xml_configs[0]+'.xml'),
            tmp_scenario_file)
        print('vm_info: ', vm_info)
    except Exception as error:
        return {'status': 'fail', 'file_name': new_scenario_name,
                'rename': rename, 'error_list': {'error': str(error)}}

    print('error_list: ', error_list)

    if not error_list:
        old_scenario_path = os.path.join(scenario_path, old_scenario_name + '.xml')
        if scenario_config_data['old_scenario_name'].startswith('user_defined_'):
            old_scenario_path = os.path.join(scenario_path, 'user_defined',
                                             old_scenario_name + '.xml')

        # check name conflict
        new_scenario_path = os.path.join(scenario_path, 'user_defined',
                                         scenario_config_data['new_scenario_name']+'.xml')
        if old_scenario_path != new_scenario_path and os.path.isfile(new_scenario_path):
            new_scenario_name = new_scenario_name + '_' + datetime.now().strftime('%Y%m%d%H%M%S')
            rename = True

        if os.path.isfile(old_scenario_path) \
                and scenario_config_data['old_scenario_name'].startswith('user_defined_'):
            os.remove(old_scenario_path)
        scenario_config.save(new_scenario_name)

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

    xml_configs = \
        get_xml_configs(launch_config_data['old_launch_name'].startswith('user_defined_'))
    launch_config = xml_configs[3]

    if xml_configs[1] is None or xml_configs[0] is None:
        return {'status': 'fail',
                'error_list': {'error': 'Please select the board info before this operation.'}}

    old_launch_name = launch_config_data['old_launch_name']
    old_launch_path = os.path.join(current_app.config.get('CONFIG_PATH'), xml_configs[1],
                                   old_launch_name + '.xml')
    if launch_config_data['old_launch_name'].startswith('user_defined_'):
        old_launch_name = launch_config_data['old_launch_name'][13:]
        old_launch_path = os.path.join(current_app.config.get('CONFIG_PATH'), xml_configs[1],
                                       'user_defined', old_launch_name + '.xml')
    launch_config.set_curr(old_launch_name)

    for key in launch_config_data:
        if key not in ['old_launch_name', 'new_launch_name', 'board_info_file',
                       'board_info_upload', 'scenario_name']:
            if isinstance(launch_config_data[key], list):
                launch_config.set_curr_list(launch_config_data[key], *tuple(key.split(',')))
            else:
                launch_config.set_curr_value(launch_config_data[key], *tuple(key.split(',')))

    scenario_name = launch_config_data['scenario_name']
    scenario_file_path = os.path.join(current_app.config.get('CONFIG_PATH'),
                                      current_app.config.get('BOARD_TYPE'), scenario_name + '.xml')
    if launch_config_data['scenario_name'].startswith('user_defined_'):
        scenario_name = launch_config_data['scenario_name'][13:]
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
    rename = False
    try:
        (error_list, pthru_sel, dm_value) = validate_launch_setting(
            os.path.join(os.path.dirname(os.path.abspath(__file__)), 'res', xml_configs[0]+'.xml'),
            scenario_file_path,
            tmp_launch_file)
        print(pthru_sel, dm_value)
    except Exception as error:
        return {'status': 'fail', 'file_name': launch_config_data['new_launch_name'],
                'rename': rename, 'error_list': {'launch config error': str(error)}}

    print('error_list: ', error_list)

    if not error_list:
        new_launch_path = os.path.join(current_app.config.get('CONFIG_PATH'),
                                       xml_configs[1], 'user_defined',
                                       launch_config_data['new_launch_name'] + '.xml')
        # check name conflict
        if old_launch_path != new_launch_path and os.path.isfile(new_launch_path):
            launch_config_data['new_launch_name'] = launch_config_data['new_launch_name'] \
                                                    + '_' + datetime.now().strftime('%Y%m%d%H%M%S')
            rename = True

        if os.path.isfile(old_launch_path) \
                and launch_config_data['old_launch_name'].startswith('user_defined_'):
            os.remove(old_launch_path)
        launch_config.save(launch_config_data['new_launch_name'])

    if os.path.isfile(tmp_launch_file):
        os.remove(tmp_launch_file)

    return {'status': 'success', 'file_name': launch_config_data['new_launch_name'],
            'rename': rename, 'error_list': error_list}


@CONFIG_APP.route('/remove_setting', methods=['POST'])
def remove_setting():
    """
    remove current setting from config app
    :return: the return message
    """
    remove_config_data = request.json if request.method == "POST" else request.args
    print("*"*100+"remove_setting")
    print(remove_config_data)

    old_setting_name = remove_config_data['old_setting_name']

    if current_app.config.get('BOARD_TYPE') is None:
        return {'status': 'Board info not set before remove current setting'}

    print(current_app.config.get('CONFIG_PATH'), current_app.config.get('BOARD_TYPE'))
    old_setting_path = os.path.join(current_app.config.get('CONFIG_PATH'),
                                    current_app.config.get('BOARD_TYPE'),
                                    old_setting_name+'.xml')
    if old_setting_name.startswith('user_defined_'):
        old_setting_path = os.path.join(current_app.config.get('CONFIG_PATH'),
                                        current_app.config.get('BOARD_TYPE'),
                                        'user_defined',
                                        old_setting_name[13:] + '.xml')

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
    print("generate_src")
    print(generator_config_data)

    src_type = generator_config_data['type']
    board_info = generator_config_data['board_info']
    board_type = current_app.config.get('BOARD_TYPE')
    board_info_xml = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                  'res', board_info+'.xml')
    scenario_setting = generator_config_data['scenario_setting']
    scenario_setting_xml = os.path.join(current_app.config.get('CONFIG_PATH'), board_type,
                                        'user_defined', scenario_setting+'.xml')
    launch_setting_xml = None
    if 'launch_setting' in generator_config_data:
        launch_setting = generator_config_data['launch_setting']
        launch_setting_xml = os.path.join(current_app.config.get('CONFIG_PATH'),
                                          board_type, 'user_defined', launch_setting + '.xml')
    commit = False
    if 'commit' in generator_config_data and generator_config_data['commit'] == 'yes':
        commit = True
    msg = {}
    error_list = {}
    status = 'success'
    if src_type == 'generate_board_src':
        try:
            from board_config.board_cfg_gen import ui_entry_api
            error_list = ui_entry_api(board_info_xml, scenario_setting_xml, commit)
        except Exception as error:
            status = 'fail'
            error_list = {'board setting error': str(error)}
    elif src_type == 'generate_scenario_src':
        try:
            from scenario_config.scenario_cfg_gen import ui_entry_api
            error_list = ui_entry_api(board_info_xml, scenario_setting_xml, commit)
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
            error_list = ui_entry_api(board_info_xml, scenario_setting_xml, launch_setting_xml, commit)
        except Exception as error:
            status = 'fail'
            error_list = {'launch setting error': str(error)}
    else:
        status = 'fail'
        error_list = {'error': 'generator type not specified'}
    msg = {'status': status, 'error_list': error_list}
    print(msg)
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

    board_type = get_board_info_type(board_info)
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


def get_xml_configs(user_defined=False):
    """
    get xml config related variables
    :return: board_info, board_config, scenario_config, launch_config
    """

    config_path = None
    board_info = current_app.config.get('BOARD_INFO')

    board_type = get_board_info_type(board_info)
    if board_type is not None:
        config_path = os.path.join(current_app.config.get('CONFIG_PATH'), board_type)

    if user_defined:
        scenario_config = XmlConfig(config_path, False)
        launch_config = XmlConfig(config_path, False)
    else:
        scenario_config = XmlConfig(config_path)
        launch_config = XmlConfig(config_path)

    return board_info, board_type, scenario_config, launch_config


def get_board_info_type(board_info):
    """
    get board info type
    :param board_info: the board info file
    :return: the board type
    """
    board_type = None
    if board_info is not None:
        board_info_config = XmlConfig(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                                   'res'))
        board_info_config.set_curr(board_info)
        board_info_root = board_info_config.get_curr_root()
        if board_info_root and 'board' in board_info_root.attrib:
            board_type = board_info_root.attrib['board']

    return board_type
