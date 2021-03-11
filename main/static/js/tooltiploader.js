$(function() {
    $('[data-toggle="tooltip"]').tooltip()

    $("#lang").change(function() {
        $("#formLang").submit();
    });
})