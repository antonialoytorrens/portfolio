from django.db import models

# Create your models here.
class Project(models.Model):
    title = models.CharField(max_length=400)
    subtitle = models.CharField(max_length=2000)
    image = models.ImageField(upload_to = 'projects/')
    body = models.TextField()
    created = models.DateTimeField(auto_now_add=True)
    modified = models.DateTimeField(auto_now=True)