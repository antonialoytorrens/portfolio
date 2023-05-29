# Blog & Portfolio
My blog and portfolio template, based on [hugo theme mini](https://github.com/nodejh/hugo-theme-mini) from [nodejh](https://nodejh.com), with a few functionalities added, such as multilanguage post and linking based on keywords.

## Linking based on keywords
If you are familiar with Hugo structure scheme, it is quite odd to have a `content_raw` folder instead of the normal `content` folder.

I make use of a custom tool that you can see here in this project, `innerlinks.py`, that scans all the Markdown files inside the `content_raw` folder and adds or removes external links based on keywords, just to not copy-paste the same link all over the Markdown files.

The reason I changed the `content` folder to the `content_raw` folder is to backup any possible mistakes the tool can make. Moreover, this tool also generates the `content` folder with all the associated inner links, so it is easy to deploy it in production and/or revert the changes in case it messed up or do not like it (just delete the `content` folder and you are done).

## Building this Hugo website
### With inner links
If you want to build this website with inner links, you also need to have Python3 installed.

To see the inner links, you will have to generate the `public` folder that we distribute in production.
With `hugo serve`, you will only see the content in `content_raw` folder, without the inner links.

* To build and export the website, open a command line and introduce the following commands:

`python3 innerlinks.py`

`hugo`

Look for the `public` folder and see if the inner links are generated.

* **Remember** that you have to modify existing content in the `content_raw` folder. All the content inside `content` will be removed every time you run the Python script! Just in case, there's a confirmation message for deleting the `content` folder.

### Without inner links
Just rename `content_raw` to `content` and build or deploy as like another Hugo project.