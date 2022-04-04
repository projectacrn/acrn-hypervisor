# Copyright (c) 2022 Anton Bobkov, Intel Corporation
# SPDX-License-Identifier: Apache-2.0

# Use git to retrieve a more meaningful last updated date than the usual
# Sphinx-generated date that's actually just the last published date.
# Inspired by a shell script by David Kinder.
#
# Add the extension to your conf.py file:
#    extensions = ['last_updated']
#
# If you copy documentation files from one or multiple git repositories prior to
# running Sphinx, specify the list of paths to the git folders in the
# last_updated_git_path parameter. You can either specify absolute paths or
# relative paths from the documentation root directory in this list. For
# example:
#
#   last_updated_git_path = ['../../', '../../../']
#
# Specify the date format in the html_last_updated_fmt parameter. For
# information about the strftime() format, see https://strftime.org.  If you do
# not specify this parameter, the extension uses the default value of "%b %d, %Y"
# for short month name, date, and four-digit year.
#
# Use the variables provided by the extension in your jinja templates:
#
#   last_updated. The date of the last update of the rst file in the specified
#      git repository.
#   last_published. The publication date of the rst file.
#
# Note: Sphinx already provides the last_updated variable. However, this
# variable includes the publication time. This extension overwrites the
# last_updated variable with the file modification time in git.
#
# To override the default footer in the sphinx_rtd_theme, create the footer.html
# file in the _templates directory:
#
# {% extends "!footer.html" %}
# {% block contentinfo %}
#
# <!-- your copyright info goes here, possibly copied from the theme's # footer.html -->
#
# <span class="lastupdated">Last updated on {{last_updated}}. Published on {{last_published}}</span>
#
# {% endblock %}
#
# This snippet overrides the contentinfo block of the default footer.html
# template that initially contains the information about the copyright and
# publication date.

__version__ = '0.1.0'

import subprocess
from datetime import date, datetime
import os
import urllib.parse
from sphinx.util import logging


def _not_git_repo(dir):
    res = subprocess.call(['git', '-C', dir, 'rev-parse'],
            stderr=subprocess.STDOUT, stdout = open(os.devnull, 'w')) != 0
    return res


def _get_last_updated_from_git(file_path, git_repo, doc_root):

    rel_path = os.path.relpath(file_path, doc_root)
    time_format = "%Y-%m-%d"

    for git_repo_path in git_repo:

        new_path = os.path.join(git_repo_path, rel_path)
        if os.path.isfile(new_path):
            try:
                    output=subprocess.check_output(
                        f'git --no-pager log -1 --date=format:"{time_format}" --pretty="format:%cd" {new_path}',
                        shell=True, cwd=git_repo_path)
            except:
                    # Could not get git info for an existing file, try the next
                    # folder on the list
                    continue
            else:
                # we found the .rst file in one of the git_repo paths, either
                # use the date of the last commit (from git) or today's date if
                # there is no git history for this file.
                try:
                    last_updated = datetime.strptime(output.decode('utf-8'), time_format).date()
                    return last_updated
                except:
                    return date.today()
        else:
            # can't find a .rst file on that git_repo path, try the next
            continue

    # falling out of the loop means we can't find that file in any of the
    # git_repo paths
    return None


def on_html_page_context(app, pagename, templatename, context, doctree):
    if doctree:

        # If last_updated_git_path (with a list of potential folders where the
        # actual git-managed files are) is not specified,
        # then just use the doc source path
        if app.config.last_updated_git_path is None:
            app.config.last_updated_git_path = [app.srcdir]

        # If last_updated_git_path is a relative path, convert it to absolute
        last_updated_git_path_abs = []
        for last_updated_git_path_el in app.config.last_updated_git_path:
            if not os.path.isabs(last_updated_git_path_el):
                last_updated_git_path_el_abs = os.path.normpath(os.path.join(app.srcdir, last_updated_git_path_el))
                last_updated_git_path_abs.append(last_updated_git_path_el_abs)
            else:
                last_updated_git_path_abs.append(last_updated_git_path_el)

            if _not_git_repo(last_updated_git_path_abs[-1]):
                app.logger.error(f"The last_updated extension is disabled because of the error:\
                        \n {last_updated_git_path_abs} is not a git repository.\
                        \n Specify correct path(s) to the git source folder(s) in last_updated_git_path.")
                app.disconnect(app.listener_id)
                return

        app.config.last_updated_git_path = last_updated_git_path_abs


        # Get the absolute path to the current rst document
        rst_file_path = doctree.attributes['source']

        # Set the date format based on html_last_updated_fmt or default of Mar 18, 2022
        if app.config.html_last_updated_fmt is None:
            date_fmt = "%b %d, %Y"
        else:
            date_fmt = app.config.html_last_updated_fmt

        context['last_published'] = date.today().strftime(date_fmt)

        last_updated_value = _get_last_updated_from_git(file_path=rst_file_path,
                                                        git_repo=app.config.last_updated_git_path,
                                                        doc_root=app.srcdir)
        if last_updated_value is None:
            app.logger.warning(f'Could not get the last updated value from git for the following file:\
                    \n {rst_file_path}\n Ensure that you specified the correct folder(s) in last_updated_git_path:\
                    \n {app.config.last_updated_git_path}\n')
            context['last_updated'] = context['last_published']
        else:
            context['last_updated'] = last_updated_value.strftime(date_fmt)



def setup(app):
    app.logger = logging.getLogger(__name__)
    app.add_config_value('last_updated_git_path', None, 'html')

    app.listener_id = app.connect('html-page-context', on_html_page_context)

    return {
            'version': '0.1',
            'parallel_read_safe': True,
    }
