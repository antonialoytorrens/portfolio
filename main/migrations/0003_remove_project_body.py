# Generated by Django 3.0.7 on 2020-09-10 14:11

from django.db import migrations


class Migration(migrations.Migration):

    dependencies = [
        ('main', '0002_auto_20200910_1348'),
    ]

    operations = [
        migrations.RemoveField(
            model_name='project',
            name='body',
        ),
    ]
