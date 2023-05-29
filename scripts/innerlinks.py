#!/usr/bin/env python3

# This is a list of common technologies and concepts (that have no translation) that I want to link automatically if some coincidence is met.
# For each and any markdown file.
import re
import os
import glob
import shutil

# Set the script path to the parent of the current working directory
script_dir = os.path.dirname(os.path.dirname(os.path.realpath(__file__)))
os.chdir(script_dir)

innerlinks = {
    # Technologies (in alphabetic order)
    "CSS": "https://wikipedia.org/wiki/CSS",
    "Bootstrap": "https://getbootstrap.com",
    "Django": "https://www.djangoproject.com",
    "Google Cloud": "https://cloud.google.com",
    "HTML": "https://wikipedia.org/wiki/HTML",
    "MongoDB": "https://wikipedia.org/wiki/MongoDB",
    "MySQL": "https://wikipedia.org/wiki/MySQL",
    "N8N": "https://n8n.io",
    "Java": "https://www.java.com",
    "JavaScript": "https://wikipedia.org/wiki/JavaScript",
    "JQuery": "https://jquery.com",
    "Kubernetes": "https://kubernetes.io",
    "PHP": "https://www.php.net/manual",
    "Python": "https://python.org",
    "Selenium": "https://www.selenium.dev",
    "Playwright": "https://playwright.dev",
    "Slim Framework": "https://www.slimframework.com/",

    # Operating Systems and Distributions (in alphabetic order)
    "Linux": "https://wikipedia.org/wiki/Linux_kernel",

    # Concepts (in alphabetic order)
    "CRUD": "https://wikipedia.org/wiki/CRUD",
    "framework": "https://wikipedia.org/wiki/Framework",

    # Programs (in alphabetic order)
    "Audacity": "https://www.audacityteam.org",
    "LMMS": "https://lmms.io",
    "Musescore": "https://musescore.org",
    "Reaper": "https://reaper.fm",
    "Sibelius7": "https://www.avid.com/sibelius",
}

# Abbreviations or alternate names
# Technologies
innerlinks["CSS3"] = innerlinks["CSS"], # CSS specification
innerlinks["Bootstrap4"] = f"{innerlinks['Bootstrap']}/docs/4.6/getting-started/introduction/", # Bootstrap 4 specification
innerlinks["HTML5"] = innerlinks["HTML"] # HTML5 specification
innerlinks["k8s"] = innerlinks["Kubernetes"] # Kubernetes abbr.
innerlinks["JS"] = innerlinks["JavaScript"] # JavaScript abbr.

# Programs
# Some notes first:
# * Beware of Sibelius, as it's also a known composer and can be wrongly linked and attributed to the program
innerlinks["Sibelius"] = innerlinks["Sibelius7"] # Generic program
innerlinks["Musescore3"] = innerlinks["Musescore"] # Versioned program

# * Note: if some link is rendered by error, add a !nolink next to the word and run the script again.
# If "linked word" !nolink is read, remove !nolink and the associated link
remove_nolinks_regex = re.compile(r'\[([^\]]+)\]\([^\)]+\)\s?!nolink')
link_pattern = re.compile(r'\[[^\]]*\]\([^\)]+\)')  # Pattern for existing markdown links
nolink_pattern = re.compile(r'\[([^\]]+)\]\([^\)]+\)\s?!nolink')  # Pattern for !nolink
def apply_links(content, innerlinks):
    chunks = link_pattern.split(content)  # Split content by existing markdown links
    link_parts = link_pattern.findall(content)  # Get the existing markdown links

    # Apply inner links only to the parts of content outside existing links
    for i in range(len(chunks)):
        for key, value in innerlinks.items():
            chunks[i] = re.sub(r'\b' + re.escape(key) + r'\b', f'[{key}]({value})', chunks[i])
    
    # Interleave the chunks and link_parts to get the final content
    final_content = ''.join(val for pair in zip(chunks, link_parts + ['']) for val in pair)

    return final_content

def remove_nolinks(content):
    # The regex pattern finds markdown links followed by !nolink and replaces it with just the linked word
    content = remove_nolinks_regex.sub(r'\1', content)
    return content

def apply_links_to_files(root_dir, innerlinks):
    for filename in glob.iglob(root_dir + '/**/*.md', recursive=True):
        # Do not modify files starting with underscore
        if os.path.basename(filename).startswith('_'):
            continue

        with open(filename, 'r') as file:
            lines = file.readlines()

        in_var_declaration = False
        for i, line in enumerate(lines):
            # Detect if we are inside a variable declaration block
            if line.strip() == '+++':
                in_var_declaration = not in_var_declaration
                continue

            # Detect if we are inside a code declaration block
            if line.strip() == '```':
                in_var_declaration = not in_var_declaration
                continue

        # If we are not in a variable declaration block, apply links
            if not in_var_declaration:
                lines[i] = apply_links(line, innerlinks)

        # Join the lines back together and remove undesired links
        content = "".join(lines)
        final_content = remove_nolinks(content)
        
        # Write back to the file
        with open(filename, 'w') as file:
            file.write(final_content)

if __name__ == "__main__":
    # Usage
    src_dir = f"{script_dir}/content_raw"
    root_dir = f"{script_dir}/content"

    # Remove content folder if exists, but avoid deleting by mistake
    if os.path.exists(root_dir) and os.path.isdir(root_dir):
        confirm = input(f"The content folder exists. Are you sure you want to permanently delete the directory at {root_dir}? (y/N): ")
        if confirm.lower() == 'y':
            shutil.rmtree(root_dir)
            print(f"Directory at {root_dir} has been deleted.")
        else:
            print("Operation cancelled.")

    # Preserve original content as backup before applying links
    shutil.copytree(src_dir, root_dir)

    apply_links_to_files(root_dir, innerlinks)