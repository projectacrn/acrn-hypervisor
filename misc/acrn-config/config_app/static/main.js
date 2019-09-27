$().ready(function(){
    $("#board_info_file").change(function () {
        var fileObj = $(this)[0].files[0];
        if (typeof (fileObj) == "undefined" || fileObj.size <= 0) {
           alert("Upload error.");
           return;
        }
        var file_name = $(this).val();
        var formFile = new FormData();
        formFile.append("name", file_name);
        formFile.append("file", fileObj);


        $.ajax({
           url: "../upload_board_info",
           data: formFile,
           type: "Post",
           dataType: "json",
           cache: false,
           processData: false,
           contentType: false,
           success: function (result) {
               console.log(result);
               if (result.status == 'success') {
                    if (result.info != 'updated') {
                        alert('Upload successfully.\nA new board type: '+result.info+' created.');
                    } else {
                        alert('Upload successfully.');
                    }
               } else {
                    alert(result.status);
               }

               window.location.reload();
           },
           error: function(e){
               console.log(e.status);
               console.log(e.responseText);
               alert(e.status+'\n'+e.responseText);
           }
       })
    });

    $("#scenario_file").change(function () {
        var fileObj = $(this)[0].files[0];
        if (typeof (fileObj) == "undefined" || fileObj.size <= 0) {
           alert("Upload error.");
           return;
        }
        var file_name = $(this).val();

        var formFile = new FormData();
        formFile.append("name", file_name);
        formFile.append("file", fileObj);

        $.ajax({
           url: "../upload_scenario",
           data: formFile,
           type: "Post",
           dataType: "json",
           cache: false,
           processData: false,
           contentType: false,
           success: function (result) {
               console.log(result);
               status = result.status;
               if (status!='success') {
                    alert(status);
                    return;
               }
               error_list = result.error_list;
               file_name = result.file_name;
               rename = result.rename
               if(result.rename==true) {
                    alert('Scenario setting existed, import successfully with a new name: '+file_name);
                } else {
                    alert('Scenario setting import successfully with name: '+file_name);
                }
               window.location = 'http://'
                        + window.location.host+"/scenario/user_defined_" + file_name;
           },
           error: function(e){
               console.log(e.status);
               console.log(e.responseText);
               alert(e.status+'\n'+e.responseText);
           }
       })
    });

    $("#launch_file").change(function () {
        var fileObj = $(this)[0].files[0];
        if (typeof (fileObj) == "undefined" || fileObj.size <= 0) {
           alert("Upload error.");
           return;
        }
        var file_name = $(this).val();

        var formFile = new FormData();
        formFile.append("name", file_name);
        formFile.append("file", fileObj);

        $.ajax({
           url: "../upload_launch",
           data: formFile,
           type: "Post",
           dataType: "json",
           cache: false,
           processData: false,
           contentType: false,
           success: function (result) {
               console.log(result);
               status = result.status;
               if (status!='success') {
                    alert(status);
                    return;
               }
               error_list = result.error_list;
               file_name = result.file_name;
               rename = result.rename
               if(result.rename==true) {
                    alert('Launch setting existed, import successfully with a new name: '+file_name);
                } else {
                    alert('Launch setting import successfully with name: '+file_name);
                }
               window.location = 'http://'
                        + window.location.host+"/launch/user_defined_" + file_name;
           },
           error: function(e){
               console.log(e.status);
               console.log(e.responseText);
               alert(e.status+'\n'+e.responseText);
           }
       })
    });

    $("select#board_info").change(function(){
        data = {board_info: $(this).val()};
        $.ajax({
            type : "POST",
            contentType: "application/json;charset=UTF-8",
            url : "../select_board",
            data : JSON.stringify(data),
            success : function(result) {
                console.log(result);
                window.location.reload(true);
            },
            error : function(e){
                console.log(e.status);
                console.log(e.responseText);
            }
        });
    });

    $("input").on('blur',function(){
        $(this).parents(".form-group").removeClass("has-error");
        $(this).parents(".form-group").children("p").text("");
    });

    $("select").on('changed.bs.select',function(){
        $(this).parents(".form-group").removeClass("has-error");
        $(this).parents(".form-group").children("p").text("");
    })

    $('#save_board').on('click', function() {
        save_board();
    });

    $('#save_scenario').on('click', function() {
        var name = $(this).data('id');
        if(name=="generate_board_src" || name=="generate_scenario_src") {
            save_scenario(name);
        }
        else {
            save_scenario();
        }
    });

    $('#remove_scenario').on('click', function() {
        old_scenario_name = $("#old_scenario_name").text();
        if(old_scenario_name.indexOf('user_defined')<0) {
            alert("Default scenario setting could not be deleted.");
            return;
        }

        var board_info = $("select#board_info").val();
        if (board_info==null || board_info=='') {
            alert("Please select one board info before this operation.");
            return;
        }

        scenario_config = {
            old_setting_name: $("#old_scenario_name").text(),
            new_setting_name: $("#new_scenario_name").val()
        }

        $.ajax({
            type : "POST",
            contentType: "application/json;charset=UTF-8",
            url : "../remove_setting",
            data : JSON.stringify(scenario_config),
            success : function(result) {
                console.log(result);
                status = result.status
                info = result.info
                if (status == 'success') {
                    alert('Remove current scenario setting from acrn-config app successfully.');
                    window.location = window.location = 'http://'
                        + window.location.host+"/scenario";
                } else {
                    alert('Remove current scenario setting from acrn-config app failed:\n'+info);
                }
            },
            error : function(e){
                console.log(e.status);
                console.log(e.responseText);
                alert(e.status+'\n'+e.responseText);
            }
        });
    });

    $('#save_launch').on('click', function() {
        var name = $(this).data('id');
        if(name=="generate_launch_script") {
            save_launch(name);
        }
        else {
            save_launch();
        }
    });

    $('#remove_launch').on('click', function() {
        old_launch_name = $("#old_launch_name").text();
        if(old_launch_name.indexOf('user_defined')<0) {
            alert("Default launch setting could not be deleted.");
            return;
        }

        var board_info = $("select#board_info").val();
        if (board_info==null || board_info=='') {
            alert("Please select one board before this operation.");
            return;
        }

        launch_config = {
            old_setting_name: $("#old_launch_name").text(),
            new_setting_name: $("#new_launch_name").val(),
        }

        $.ajax({
            type : "POST",
            contentType: "application/json;charset=UTF-8",
            url : "../remove_setting",
            data : JSON.stringify(launch_config),
            success : function(result) {
                console.log(result);
                status = result.status
                info = result.info
                if (status == 'success') {
                    alert('Remove current launch setting from acrn-config app successfully.');
                    window.location = window.location = 'http://'
                        + window.location.host+"/launch";
                } else {
                    alert('Remove current launch setting from acrn-config app failed:\n'+info);
                }
            },
            error : function(e){
                console.log(e.status);
                console.log(e.responseText);
                alert(e.status+'\n'+e.responseText);
            }
        });
    });

    $('#generate_board_src').on('click', function() {
        var dataId = $(this).data('id');
        $("#save_scenario").data('id', dataId);
    });

    $('#generate_scenario_src').on('click', function() {
        var dataId = $(this).data('id');
        $("#save_scenario").data('id', dataId);
    });

    $('#generate_launch_script').on('click', function() {
        var dataId = $(this).data('id');
        $("#save_launch").data('id', dataId);
    });

    $("select[ID$='vuart:id=1,base']").change(function(){

        var id = $(this).attr('id');
        var value = $(this).val();
        show_com_target(id, value);
    });

    $("select[ID$='vuart:id=1,base']").each(function(index, item) {
        var id = $(item).attr('id');
        var value = $(item).val();
        show_com_target(id, value);
    })

})


function show_com_target(id, value) {

    if(id==null || id=='undefined') {
        return
    }
    var id2 = id.replace('base', 'target_vm_id');
    var jquerySpecialChars = ["~", "`", "@", "#", "%", "&", "=", "'", "\"",
        ":", ";", "<", ">", ",", "/"];
    for (var i = 0; i < jquerySpecialChars.length; i++) {
        id2 = id2.replace(new RegExp(jquerySpecialChars[i],
            "g"), "\\" + jquerySpecialChars[i]);
    }
    if (value == 'INVALID_COM_BASE') {
        $('#'+id2+'_label1').hide();
        $('#'+id2+'_label2').hide();
        $('#'+id2+'_config').hide();
        $('#'+id2+'_err').hide();
    }
    else {
        $('#'+id2+'_label1').show();
        $('#'+id2+'_label2').show();
        $('#'+id2+'_config').show();
        $('#'+id2+'_err').show();
    }
}


function save_scenario(generator=null){
    var board_info = $("select#board_info").val();
    if (board_info==null || board_info=='') {
        alert("Please select one board info before this operation.");
        return;
    }

    scenario_config = {
		old_scenario_name: $("#old_scenario_name").text(),
		new_scenario_name: $("#new_scenario_name").val()
	}

    $("input").each(function(){
        var id = $(this).attr('id');
    	var value = $(this).val();
    	if(id!='new_scenario_name' && id!='board_info_file'
    	    && id!='board_info_upload' && id!="scenario_file") {
    	    scenario_config[id] = $(this).val();
    	}
    })

    $("textarea").each(function(){
        var id = $(this).attr('id');
    	var value = $(this).val();
    	if(id!='new_scenario_name' && id!='board_info_file'
    	    && id!='board_info_upload' && id!="scenario_file") {
    	    scenario_config[id] = $(this).val();
    	}
    })

    $("select").each(function(){
        var id = $(this).attr('id');
    	var value = $(this).val();
    	if(id!='board_info') {
    	    scenario_config[$(this).attr('id')] = $(this).val();
    	}
    })

    $.ajax({
        type : "POST",
        contentType: "application/json;charset=UTF-8",
        url : "../save_scenario",
        data : JSON.stringify(scenario_config),
        success : function(result) {
            error_list = result.error_list;
            status = result.status;
            var no_err = true;
            $.each(error_list, function(index,item){
                    no_err = false;
                    var jquerySpecialChars = ["~", "`", "@", "#", "%", "&", "=", "'", "\"",
                        ":", ";", "<", ">", ",", "/"];
                    for (var i = 0; i < jquerySpecialChars.length; i++) {
                        index = index.replace(new RegExp(jquerySpecialChars[i],
                            "g"), "\\" + jquerySpecialChars[i]);
                    }
                    $("#"+index).parents(".form-group").addClass("has-error");
                    $("#"+index+"_err").text(item);
            })
            if(no_err == true && status == 'success') {
                file_name = result.file_name;
                if(result.rename==true) {
                    alert('Scenario setting existed, saved successfully with a new name: '
                        +file_name+'\nto acrn-hypervisor/misc/acrn-config/xmls/config-xmls/'+board_info+'/user_defined/');
                } else {
                    alert('Scenario setting saved successfully with name: '
                        +file_name+'\nto acrn-hypervisor/misc/acrn-config/xmls/config-xmls/'+board_info+'/user_defined/');
                }
                if(generator != null) {
                    generator_config = {
                        type: generator,
                        board_info: $("select#board_info").val(),
                        board_setting: "board_setting",
                        scenario_setting: file_name
                    }
                    $.ajax({
                        type : "POST",
                        contentType: "application/json;charset=UTF-8",
                        url : "../generate_src",
                        data : JSON.stringify(generator_config),
                        success : function(result) {
                            console.log(result);
                            status = result.status
                            error_list = result.error_list
                            if (status == 'success' && JSON.stringify(error_list)=='{}') {
                                alert(generator+' successfully.');
                            } else {
                                alert(generator+' failed. \nError list:\n'+JSON.stringify(error_list));
                            }
                            window.location = "./user_defined_" + file_name;
                        },
                        error : function(e){
                            console.log(e.status);
                            console.log(e.responseText);
                            alert(e.status+'\n'+e.responseText);
                        }
                    });
                } else {
                    window.location = "./user_defined_" + file_name;
                }
            }
            else if(status != 'success') {
                alert(JSON.stringify(error_list));
            }
        },
        error : function(e){
            console.log(e.status);
            console.log(e.responseText);
            alert(e.status+'\n'+e.responseText);
        }
    });
}

function save_launch(generator=null) {
    var board_info = $("select#board_info").val();
    var scenario_name = $("select#scenario_name").val();
    if (board_info==null || board_info=='' || scenario_name==null || scenario_name=='') {
        alert("Please select one board and scenario before this operation.");
        return;
    }

    launch_config = {
		old_launch_name: $("#old_launch_name").text(),
		new_launch_name: $("#new_launch_name").val(),
		scenario_name: scenario_name
	}

    $("input").each(function(){
        var id = $(this).attr('id');
    	var value = $(this).val();
    	if(id!='new_launch_name' && id!='board_info_file'
    	    && id!='board_info_upload' && id!='scenario_name'
    	    && id!="launch_file") {
    	    launch_config[id] = $(this).val();
    	}
    })

    $("select").each(function(){
        var id = $(this).attr('id');
    	var value = $(this).val();
    	if(id!='board_info') {
    	    launch_config[$(this).attr('id')] = $(this).val();
    	}
    })

    $("textarea").each(function(){
        var id = $(this).attr('id');
    	var value = $(this).val();
    	if(id!='new_scenario_name' && id!='board_info_file'
    	    && id!='board_info_upload' && id!="scenario_file") {
    	    launch_config[id] = $(this).val();
    	}
    })

    $.ajax({
        type : "POST",
        contentType: "application/json;charset=UTF-8",
        url : "../save_launch",
        data : JSON.stringify(launch_config),
        success : function(result) {
            console.log(result);
            error_list = result.error_list;
            status = result.status;

            var no_err = true;
            $.each(error_list, function(index,item){
                    no_err = false;
                    var jquerySpecialChars = ["~", "`", "@", "#", "%", "&", "=", "'", "\"",
                        ":", ";", "<", ">", ",", "/"];
                    for (var i = 0; i < jquerySpecialChars.length; i++) {
                        index = index.replace(new RegExp(jquerySpecialChars[i],
                            "g"), "\\" + jquerySpecialChars[i]);
                    }
                    $("#"+index).parents(".form-group").addClass("has-error");
                    $("#"+index+"_err").text(item);
            })
            if(no_err == true && status == 'success') {
                file_name = result.file_name;
                if(result.rename==true) {
                    alert('Launch setting existed, saved successfully with a new name: '
                        +file_name+'\nto acrn-hypervisor/misc/acrn-config/xmls/config-xmls/'+board_info+'/user_defined/');
                } else {
                    alert('Launch setting saved successfully with name: '
                        +file_name+'\nto acrn-hypervisor/misc/acrn-config/xmls/config-xmls/'+board_info+'/user_defined/');
                }
                if(generator != null) {
                    generator_config = {
                        type: generator,
                        board_info: $("select#board_info").val(),
                        board_setting: "board_setting",
                        scenario_setting: $("select#scenario_name").val(),
                        launch_setting: file_name
                    }
                    $.ajax({
                        type : "POST",
                        contentType: "application/json;charset=UTF-8",
                        url : "../generate_src",
                        data : JSON.stringify(generator_config),
                        success : function(result) {
                            console.log(result);
                            status = result.status
                            error_list = result.error_list
                            if (status == 'success' && JSON.stringify(error_list)=='{}') {
                                alert(generator+' successfully.');
                            } else {
                                alert(generator+' failed. \nError list:\n'+JSON.stringify(error_list));
                            }
                            window.location = "./user_defined_" + file_name;
                        },
                        error : function(e){
                            console.log(e.status);
                            console.log(e.responseText);
                            alert(e.status+'\n'+e.responseText);
                        }
                    });
                } else {
                    window.location = "./user_defined_" + file_name;
                }
            }
            else if(status != 'success') {
                alert(JSON.stringify(error_list));
            }
        },
        error : function(e){
            console.log(e.status);
            console.log(e.responseText);
            alert(e.status+'\n'+e.responseText);
        }
    });
}
