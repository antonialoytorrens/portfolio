from django.shortcuts import render

# Create your views here.
from django.shortcuts import render
from django.views.generic import TemplateView
#from .models import Project

# Create your views here.
class PortfolioList(TemplateView):
    template_name = "index.html"

    def get_context_data(self, **kwargs):
        context = super().get_context_data(**kwargs)
        #projects = Project.objects.all().filter(visible=True)
        #context["projects"] = projects
        return context
