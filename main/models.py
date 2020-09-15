from django.db import models

# Create your models here.
class Project(models.Model):
    title = models.CharField(max_length=400)
    subtitle = models.CharField(max_length=2000)
    image = models.ImageField(upload_to = 'projects/')
    alt = models.CharField(max_length=400)
    longdesc = models.CharField(max_length=2000)
    web_link = models.CharField(max_length=2000, null=True, blank=True)
    web_name = models.CharField(max_length=500, blank=True, null=True)
    created = models.DateTimeField()
    modified = models.DateTimeField(auto_now=True)