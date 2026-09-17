$(function() {
    $('[data-toggle="tooltip"]').tooltip()

    $("#lang").change(function() {
        window.location = "/" + $(this).val() + "/";
    });
})
