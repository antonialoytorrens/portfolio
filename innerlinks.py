# This is a list of common technologies and concepts (that have no translation) that I want to link automatically if some coincidence is met.
# For each and any markdown file.
import re
import os
import glob

# Set the script path to the current working directory
script_dir = os.path.dirname(os.path.realpath(__file__))
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

# If some link is rendered by error, add a !nolink next to the word.
# If "linked word" !nolink is read, remove !nolink and the associated link
remove_nolinks_regex = re.compile(r'\[([^\]]+)\]\([^\)]+\)\s?!nolink')
def apply_links(content, innerlinks):
    for key, value in innerlinks.items():
        content = content.replace(key, f'[{key}]({value})')
    return content

def remove_nolinks(content):
    # The regex pattern finds markdown links followed by !nolink and replaces it with just the linked word
    content = remove_nolinks_regex.sub(r'\1', content)
    return content

def apply_links_to_files(root_dir, innerlinks):
    for filename in glob.iglob(root_dir + '**/*.md', recursive=True):
        with open(filename, 'r') as file:
            content = file.read()
        
        # Apply the links
        linked_content = apply_links(content, innerlinks)

        # Remove undesired links
        final_content = remove_nolinks(linked_content)
        
        # Write back to the file
        with open(filename, 'w') as file:
            file.write(final_content)

# Usage
root_dir = "public/"
apply_links_to_files(root_dir, innerlinks)