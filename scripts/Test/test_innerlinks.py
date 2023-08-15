#!/usr/bin/env python3

import re
import os
import glob
import shutil
import pytest
from sys import path

# Add the parent directory to sys.path
path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))

from innerlinks import apply_links, apply_links_to_files, remove_nolinks

# * BEGIN SETUP
# This is the test suite done for the innerlinks.py tool to ensure reliability.

# Set the script path to the parent of the current working directory
script_dir = f"{os.path.dirname(os.path.realpath(__file__))}"
os.chdir(script_dir)

src_dir = f"{script_dir}/original"
root_dir = f"{script_dir}/input"
check_dir = f"{script_dir}/tested_output"

innerlinks_test_dict = innerlinks = {
    # Technologies (in alphabetic order)
    "CSS": "https://wikipedia.org/wiki/CSS",
    "Java": "https://www.java.com",
    "JavaScript": "https://wikipedia.org/wiki/JavaScript"
}

# Abbreviations or alternate names
# Technologies
innerlinks_test_dict["CSS3"] = innerlinks_test_dict["CSS"] # CSS specification
innerlinks_test_dict["JS"] = innerlinks_test_dict["JavaScript"] # JavaScript abbr.

# Remove tested content if the test has been run before
try:
    shutil.rmtree(root_dir)
except FileNotFoundError:
    pass

# Preserve original content as backup before applying links
shutil.copytree(src_dir, root_dir)

# Run `innerlinks.py``
apply_links_to_files(root_dir, innerlinks_test_dict)
# * END SETUP

# * BEGIN HELPER METHODS
def are_files_equal(file_to_test, tested_file):
    # Open the files
    with open(file_to_test, 'r') as file1, open(tested_file, 'r') as file2:
        # Read the contents
        content1 = file1.read()
        content2 = file2.read()

    # Return True if the contents are equal, False otherwise
    return content1 == content2
# * END HELPER METHODS

# * BEGIN TESTS
test_params = [(root_dir, check_dir)]
@pytest.mark.parametrize("root_dir, check_dir", test_params)
def test_innerlinks(root_dir: str, check_dir: str):
    """
    In all of the cases: Compare generated markdown vs manually parsed markdown file.
    1. Scan a markdown file that does not have any keyword.
        File: nokeyword.md
    2. Scan a markdown file that has one keyword next to each other.
        File: twokeywordsnext.md
    3. Test that we do not have two equal keywords 'JS' with one link and later 'JS' with other link (just throw a warning)
        File: N/A
    4. Scan a markdown file that does not contain the exact keyword, but it is similar (should not generate link). For example: 'JS' with 'JS6'. 'JS6' should not be link.
        File: similarkeyword.md
    5. Test that we do not parse the keywords inside +++ (variable declaration in Hugo).
        File: noparseplus.md
    6. Test that we do not parse the keywords inside ``` (code block declaration in markdown).
        File: noparsecode.md
    7. Test that a link will not be generated in case that we already have a link inside the word. For example, we want to override the keyword 'JS' with another link.
        File: nolinkinlink.md
    8. Test that abbreviations and altnames also work.
        File: keywordabbr.md
    9. Test that a keyword with !nolink next to it is not parsed. e.g.: 'JS !nolink'. Test also that 'JS' has an inner link when 'JS hello !nolink'
        File: keywordnolink.md
        File: keywordnokeywordnolink.md
    Last. Test a markdown file with all the features above (except step 1.) (multiple times) and see we do parse the variables.
    """
    files_to_check = ["nokeyword.md", "twokeywordsnext.md", "similarkeyword.md", "noparseplus.md", "nolinkinlink.md", "keywordabbr.md", "keywordnolink.md", "keywordnokeywordnolink.md"]
    for file_to_check in files_to_check:
        # Run tests
        assert are_files_equal(f"{root_dir}/{file_to_check}", f"{check_dir}/{file_to_check}") == True
    # TODO: test 3
    # TODO: misplaced !nolink in custom link
    # [JavaScript](https://wikipedia.org/wiki/JavaScript) hello !nolink. == [JavaScript](https://wikipedia.org/wiki/JavaScript) hello !nolink.
    
# * END TESTS