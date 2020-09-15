from django.contrib import admin
from .models import Project

# Register your models here.

class ProjectAdmin(admin.ModelAdmin):
    list_display = ('title_ca', 'title_es', 'title_en', 'created', 'modified')

    exclude = ('title', 'subtitle', 'alt', 'longdesc', 'web_name')

admin.site.register(Project, ProjectAdmin)