# translations.mk - Translation mappings for multilingual site

# Consolidated translation mappings: key,ca,es,en
I18N_MENU_MAPPINGS = \
	education,educacio,educacion,education \
	about,sobre-mi,acerca-de-mi,about-me \
	contact,contacte,contacto,contact \
	projects,projectes,proyectos,projects \
	portfolio,portfoli,portafolio,portfolio \
	blog,blog,blog,blog \
	index,index,index,index \
	$(NULL)

# Define comma for use in functions
comma = ,

# Extract all page keys from I18N_MENU_MAPPINGS
PAGE_KEYS = $(foreach mapping,$(I18N_MENU_MAPPINGS),$(word 1,$(subst $(comma), ,$(mapping))))

# Language to position mapping function
# Usage: $(call get_lang_pos,ca) returns 2
define get_lang_pos
$(strip $(if $(filter $1,key),1,$(if $(filter $1,ca),2,$(if $(filter $1,es),3,$(if $(filter $1,en),4,)))))
endef

# Helper function to get nth word from comma-separated list
# Usage: $(call get_word_csv,education$(comma)educacio$(comma)educacion$(comma)education,2)
get_word_csv = $(word $2,$(subst $(comma), ,$1))

# Function to find mapping line by key
# Usage: $(call find_mapping,education)
define find_mapping
$(strip $(foreach mapping,$(I18N_MENU_MAPPINGS),$(if $(filter $1,$(word 1,$(subst $(comma), ,$(mapping)))),$(mapping))))
endef

# Function to get page name for specific language
# Usage: $(call get_page_name,education,ca)
define get_page_name
$(call get_word_csv,$(call find_mapping,$1),$(call get_lang_pos,$2))
endef

# Function to find page key by name and language
# Usage: $(call find_page_key,educacio,ca)
define find_page_key
$(strip $(foreach mapping,$(I18N_MENU_MAPPINGS),\
$(if $(filter $1,$(call get_word_csv,$(mapping),$(call get_lang_pos,$2))),\
$(word 1,$(subst $(comma), ,$(mapping))))))
endef

# Function to generate all menu variables for a specific language
# It defines MENU_KEY = lang/slug for each page key:
# MENU_EDUCATION = ca/educacio; MENU_ABOUT = ca/sobre-mi; MENU_CONTACT = ca/contacte
# Usage: $(eval $(call generate_menu_vars,ca))
define generate_menu_vars
$(foreach key,$(PAGE_KEYS),\
MENU_$(shell echo $(key) | tr a-z A-Z) = $1/$(call get_page_name,$(key),$1)
)
endef

# Function to generate ALL translation variables for a page build
# Usage: $(call get_page_translations,educacio,ca)
define get_page_translations
$(strip $(if $(call find_page_key,$1,$2),\
    -D CURRENT_URL_KEY=$(call find_page_key,$1,$2) \
	-D CURRENT_URL=/$2/$(call get_page_name,$(call find_page_key,$1,$2),$2) \
    $(foreach lang,$(LANGS),-D CURRENT_URL_$(shell echo $(lang) | tr a-z A-Z)=/$(lang)/$(call get_page_name,$(call find_page_key,$1,$2),$(lang))) \
    $(foreach key,$(PAGE_KEYS),-D URL_$(shell echo $(key) | tr a-z A-Z)=/$2/$(call get_page_name,$(key),$2)) \
))
endef
