# Copyright (C) 2019 Intel Corporation.
# SPDX-License-Identifier: BSD-3-Clause

"""Entry for config app.

"""

import os
import sys
import threading
import webbrowser

# flask: Copyright 2010 Pallets
# SPDX-License-Identifier: BSD-3-Clause
# Refer to https://github.com/pallets/flask/blob/master/LICENSE.rst for the permission notice.
from flask import Flask

# flask: Copyright (c) 2013, Marc Brinkmann
# SPDX-License-Identifier: BSD-3-Clause
# Refer to https://pypi.org/project/Flask-Bootstrap/ for the permission notice.
from flask_bootstrap import Bootstrap

import configs
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'library'))
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..',
                             'board_config'))
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..',
                             'scenario_config'))
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..',
                             'launch_config'))
from views import CONFIG_APP

APP = Flask(__name__)
APP.config.from_object(configs)
APP.register_blueprint(CONFIG_APP)
APP.jinja_env.add_extension('jinja2.ext.do')
Bootstrap(app=APP)

if __name__ == '__main__':
    URL = "http://127.0.0.1:5001/scenario"
    threading.Timer(1, lambda: webbrowser.open(URL)).start()
    APP.run(port=5001, debug=False)
