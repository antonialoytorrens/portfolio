from modeltranslation.translator import translator, TranslationOptions
from .models import Project

class ProjectTranslationOptions(TranslationOptions):
    fields = ('title','subtitle','alt','longdesc','web_name',)
    required_languages = ('ca', 'es', 'en')

translator.register(Project, ProjectTranslationOptions)