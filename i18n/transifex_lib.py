# -*- coding: utf-8 -*-

# NOTE: The master copy of this file is in the psiphon-circumvention-system repo.
# TODO: Find a better place, and/or create a real library.

# Copyright (c) 2017, Psiphon Inc.
# All rights reserved.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

'''
Pulls and massages our translations from Transifex.
'''

import os
import sys
import errno
import shutil
import json
import codecs
import argparse
import requests
import localizable
from bs4 import BeautifulSoup

# To install this dependency on macOS:
# pip install --upgrade setuptools --user python
# pip install --upgrade ruamel.yaml --user python
from ruamel.yaml import YAML
from ruamel.yaml.compat import StringIO
# From https://yaml.readthedocs.io/en/latest/example.html#output-of-dump-as-a-string


class YAML_StringDumper(YAML):
    """Used for dumping YAML to a string.
    """

    def dump(self, data, stream=None, **kw):
        inefficient = False
        if stream is None:
            inefficient = True
            stream = StringIO()
        YAML.dump(self, data, stream, **kw)
        if inefficient:
            return stream.getvalue()


# If an unused translation reaches this % completion, a notice will be printed about it.
TRANSLATION_COMPLETION_PRINT_THRESHOLD = 50


# Used when keeping track of untranslated strings during merging.
UNTRANSLATED_FLAG = '[UNTRANSLATED]'


# Transifex credentials.
# Must be of the form:
# {"username": ..., "password": ...}
_config = None  # Don't use this directly. Call _getconfig()


def get_config():
    global _config
    if _config:
        return _config

    DEFAULT_CONFIG_FILENAME = 'transifex_conf.json'

    # Figure out where the config file is
    parser = argparse.ArgumentParser(
        description='Pull translations from Transifex')
    parser.add_argument('configfile', default=None, nargs='?',
                        help='config file (default: pwd or location of script)')
    args = parser.parse_args()
    configfile = None
    if args.configfile and os.path.exists(args.configfile):
        # Use the script argument
        configfile = args.configfile
    elif os.path.exists(DEFAULT_CONFIG_FILENAME):
        # Use the conf in pwd
        configfile = DEFAULT_CONFIG_FILENAME
    elif __file__ and os.path.exists(os.path.join(
            os.path.dirname(os.path.realpath(__file__)),
            DEFAULT_CONFIG_FILENAME)):
        configfile = os.path.join(
            os.path.dirname(os.path.realpath(__file__)),
            DEFAULT_CONFIG_FILENAME)
    else:
        print('Unable to find config file')
        sys.exit(1)

    with open(configfile) as config_fp:
        _config = json.load(config_fp)

    if not _config:
        print('Unable to load config contents')
        sys.exit(1)

    return _config


def process_resource(resource, langs, master_fpath, output_path_fn, output_mutator_fn,
                     bom=False, encoding='utf-8'):
    """
    Pull translations for `resource` from transifex. Languages in `langs` will be
    pulled.
    `master_fpath` is the file path to the master (English) language version of the resource.
    `output_path_fn` must be callable. It will be passed the language code and
    must return the path+filename to write to.
    `output_mutator_fn` must be callable. It will be passed `master_fpath, lang, fname, translation`
    and must return the resulting translation. May be None.
    If `bom` is True, the file will have a BOM. File will be encoded with `encoding`.
    """

    print(f'\nResource: {resource}')

    # Check for high-translation languages that we won't be pulling
    stats = transifex_request(f'resource/{resource}/stats')
    for lang in stats:
        if int(stats[lang]['completed'].rstrip('%')) >= TRANSLATION_COMPLETION_PRINT_THRESHOLD:
            if lang not in langs and lang != 'en':
                print((
                    f'Skipping language "{lang}" '
                    f'with {stats[lang]["completed"]} translation '
                    f'({stats[lang]["translated_entities"]} of '
                    f'{stats[lang]["translated_entities"] + stats[lang]["untranslated_entities"]})'))

    for in_lang, out_lang in list(langs.items()):
        r = transifex_request(f'resource/{resource}/translation/{in_lang}')

        output_path = output_path_fn(out_lang)

        # Make sure the output directory exists.
        try:
            os.makedirs(os.path.dirname(output_path))
        except OSError as ex:
            if ex.errno == errno.EEXIST and os.path.isdir(os.path.dirname(output_path)):
                pass
            else:
                raise

        if output_mutator_fn:
            content = output_mutator_fn(
                master_fpath, out_lang, output_path, r['content'])
        else:
            content = r['content']

        # Make line endings consistently Unix-y.
        content = content.replace('\r\n', '\n')

        with codecs.open(output_path, 'w', encoding) as f:
            if bom:
                f.write('\N{BYTE ORDER MARK}')

            f.write(content)


def transifex_request(command, params=None):
    """Make a request to the Transifex API.
    """

    url = 'https://www.transifex.com/api/2/project/Psiphon3/' + command + '/'
    r = requests.get(url, params=params,
                     auth=(get_config()['username'], get_config()['password']))
    if r.status_code != 200:
        raise Exception(f'Request failed with code {r.status_code}: {url}')
    return r.json()


#
# Helpers for merging different file types.
#
# Often using an old translation is better than reverting to the English when
# a translation is incomplete. So we'll merge old translations into fresh ones.
#
# All of merge_*_translations functions have the same signature:
#   `master_fpath`: The filename and path of the master language file (i.e., English).
#   `trans_lang`: The translation language code (as used in the filename).
#   `trans_fpath`: The translation filename and path.
#   `fresh_raw`: The raw content of the new translation.
# Note that all paths can be relative to cwd.
#
# All of the flag_untranslated_* functions have the same signature:
#

def merge_yaml_translations(master_fpath, lang, trans_fpath, fresh_raw):
    """Merge YAML files (such as are used by Store Assets).
    """

    yml = YAML_StringDumper()
    yml.encoding = None  # unicode, which we'll encode when writing the file

    fresh_translation = yml.load(fresh_raw)

    with codecs.open(master_fpath, encoding='utf-8') as f:
        english_translation = yml.load(f)

    try:
        with codecs.open(trans_fpath, encoding='utf-8') as f:
            existing_translation = yml.load(f)
    except Exception as ex:
        print(f'merge_yaml_translations: failed to open existing translation: {trans_fpath} -- {ex}\n')
        return fresh_raw

    # Transifex does not populate YAML translations with the English fallback
    # for missing values.

    for key in english_translation['en']:
        if not fresh_translation[lang].get(key) and existing_translation[lang].get(key):
            fresh_translation[lang][key] = existing_translation[lang].get(key)

    return yml.dump(fresh_translation)


def merge_applestrings_translations(master_fpath, lang, trans_fpath, fresh_raw):
    """Merge Xcode `.strings` files.
    """

    # First flag all the untranslated entries, for later reference.
    fresh_raw = _flag_untranslated_applestrings(
        master_fpath, lang, trans_fpath, fresh_raw)

    fresh_translation = localizable.parse_strings(content=fresh_raw)
    english_translation = localizable.parse_strings(filename=master_fpath)

    try:
        existing_translation = localizable.parse_strings(filename=trans_fpath)
    except Exception as ex:
        print(f'merge_applestrings_translations: failed to open existing translation: {trans_fpath} -- {ex}\n')
        return fresh_raw

    fresh_merged = ''

    for entry in fresh_translation:
        try:
            english = next(x['value']
                           for x in english_translation if x['key'] == entry['key'])
        except:
            english = None

        try:
            existing = next(
                x for x in existing_translation if x['key'] == entry['key'])

            # Make sure we don't fall back on an untranslated value. See comment
            # on function `flag_untranslated_*` for details.
            if UNTRANSLATED_FLAG in existing['comment']:
                existing = None
            else:
                existing = existing['value']
        except:
            existing = None

        fresh_value = entry['value']

        if fresh_value == english and existing is not None and existing != english:
            # The fresh translation has the English fallback
            fresh_value = existing

        escaped_fresh = fresh_value.replace('"', '\\"').replace('\n', '\\n')

        fresh_merged += f'/*{entry["comment"]}*/\n"{entry["key"]}" = "{escaped_fresh}";\n\n'

    return fresh_merged


def _flag_untranslated_applestrings(master_fpath, lang, trans_fpath, fresh_raw):
    """
    When retrieved from Transifex, Apple .strings files include all string table
    entries, with the English provided for untranslated strings. This counteracts
    our efforts to fall back to previous translations when strings change. Like so:
    - Let's say the entry `"CANCEL_ACTION" = "Cancel";` is untranslated for French.
      It will be in the French strings file as the English.
    - Later we change "Cancel" to "Stop" in the English, but don't change the key.
    - On the next transifex_pull, this script will detect that the string is untranslated
      and will look at the previous French "translation" -- which is the previous
      English. It will see that that string differs and get fooled into thinking
      that it's a valid previous translation.
    - The French UI will keep showing "Cancel" instead of "Stop".

    While pulling translations, we are going to flag incoming non-translated strings,
    so that we can check later and not use them a previous translation. We'll do
    this "flagging" by putting the string "[UNTRANSLATED]" into the string comment.

    (An alternative approach that would also work: Remove any untranslated string
    table entries. But this seems more drastic than modifying a comment could have
    unforeseen side-effects.)
    """

    fresh_translation = localizable.parse_strings(content=fresh_raw)
    english_translation = localizable.parse_strings(filename=master_fpath)
    fresh_flagged = ''

    for entry in fresh_translation:
        try:
            english = next(x['value']
                           for x in english_translation if x['key'] == entry['key'])
        except:
            english = None

        if entry['value'] == english:
            # The string is untranslated, so flag the comment
            entry['comment'] = UNTRANSLATED_FLAG + entry['comment']

        entry['value'] = entry['value'].replace(
            '"', '\\"').replace('\n', '\\n')

        fresh_flagged += f'/*{entry["comment"]}*/\n"{entry["key"]}" = "{y["value"]}";\n\n'

    return fresh_flagged


#
# Helpers for specific file types
#

def yaml_lang_change(to_lang, _, in_yaml):
    """
    Transifex doesn't support the special character-type modifiers we need for some
    languages, like 'ug' -> 'ug@Latn'. So we'll need to hack in the  character-type info.
    """
    return to_lang + in_yaml[in_yaml.find(':'):]
