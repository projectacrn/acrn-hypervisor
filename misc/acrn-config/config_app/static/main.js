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
                        + window.location.host+"/scenario/" + file_name;
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
                        + window.location.host+"/launch/" + file_name;
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
        if(name=="generate_config_src") {
            save_scenario(name);
        }
        else {
            save_scenario();
        }
    });

    $('#remove_scenario').on('click', function() {
        old_scenario_name = $("#old_scenario_name").text();

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

    $('#export_scenario_xml').on('click', function() {
        var dataId = $(this).data('id');
        $("#save_scenario").data('id', dataId);
        $('#src_path_row').addClass('hidden');
    });

    $('#generate_config_src').on('click', function() {
        var dataId = $(this).data('id');
        $("#save_scenario").data('id', dataId);
        $('#src_path_row').removeClass('hidden');
    });

    $('#export_launch_xml').on('click', function() {
        var dataId = $(this).data('id');
        $("#save_launch").data('id', dataId);
        $('#src_path_row').addClass('hidden');
    });


    $('#generate_launch_script').on('click', function() {
        var dataId = $(this).data('id');
        $("#save_launch").data('id', dataId);
        $('#src_path_row').removeClass('hidden');
    });

    $('a.create_menu').on('click', function() {
        var type = $(this).data('id');
        $("#createModalLabel").text("Create a new " + type + " setting");
        var date = new Date();
        $("#create_name").val(date.getTime());
        $("#create_btn").data('id', type);
    });

    $('#create_btn').on('click', function() {
        var type = $(this).data('id');
        var create_name = $("#create_name").val();
        create_setting(type, create_name, create_name, 'create');
    });

    $(document).on('change', "select#load_scenario_name", function() {
        $('input#load_scenario_name2').val(this.value);
    });

    $(document).on('change', "select#load_launch_name", function() {
        $('input#load_launch_name2').val(this.value);
    });

    $('#load_scenario_btn').on('click', function() {
        var type = $(this).data('id');
        var default_load_name = $("#load_scenario_name").val();
        var load_name = $("#load_scenario_name2").val();
        create_setting(type, default_load_name, load_name, 'load')
    });

    $('#load_launch_btn').on('click', function() {
        var type = $(this).data('id');
        var default_load_name = $("#load_launch_name").val();
        var load_name = $("#load_launch_name2").val();
        create_setting(type, default_load_name, load_name, 'load')
    });

    $(document).on('click', "#add_vm", function() {
        var curr_vm_id = $(this).data('id');
        $("#add_vm_submit").data('id', curr_vm_id);
    });

    $(document).on('click', "#add_vm_submit", function() {
        var curr_vm_id = $(this).data('id');
        save_scenario('add_vm:'+curr_vm_id)
    });

    $(document).on('click', "#remove_vm", function() {
        var remove_confirm_message = 'Do you want to delete this VM?'
        if(confirm(remove_confirm_message)) {
            var curr_vm_id = $(this).data('id');
            save_scenario('remove_vm:'+curr_vm_id)
        }
    });

    $(document).on('click', "#add_launch_vm", function() {
        var curr_vm_id = $(this).data('id');
        $("#add_launch_submit").data('id', curr_vm_id);
    });

    $(document).on('click', "#add_launch_submit", function() {
        var curr_vm_id = $(this).data('id');
        save_launch('add_vm:'+curr_vm_id);
    });

    $('#add_launch_script').on('click', function() {
        var curr_vm_id = $(this).data('id');
        $("#add_launch_submit").data('id', curr_vm_id);
    });

    $(document).on('click', "#remove_launch_vm", function() {
        var remove_confirm_message = 'Do you want to delete this VM?'
        if(confirm(remove_confirm_message)) {
            var curr_vm_id = $(this).data('id');
            save_launch('remove_vm:'+curr_vm_id)
        }
    });

    $(document).on('change', "select#scenario_name", function() {
        data = {scenario_name: $(this).val(),
                launch_name: $('text#old_launch_name').text()};
        $.ajax({
            type : "POST",
            contentType: "application/json;charset=UTF-8",
            url : "../get_post_launch_vms",
            data : JSON.stringify(data),
            success : function(result) {
                console.log(result);
                vm_list = result.vm_list
                $('select#add_launch_type').empty().selectpicker('refresh');
                for(i in vm_list) {
                    var option = vm_list[i][1]+' ( ID : '+vm_list[i][0]+' )'
                    $('select#add_launch_type').append(
                        '<option value="'+option+'">'+option+'</option>').selectpicker('refresh');
                }
            },
            error : function(e){
                console.log(e.status);
                console.log(e.responseText);
            }
        });
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
    });

    $("select[ID$='FEATURES,RDT,CDP_ENABLED']").change(function(){
        var id = $(this).attr('id');
        var value = $(this).val();
        update_vcpu_clos_option(id, value);
        update_rdt_clos_mask(id, value);
    });

    $("select[ID$='FEATURES,RDT,CDP_ENABLED']").each(function(index, item) {
        var id = $(this).attr('id');
        var value = $(item).val();
        update_vcpu_clos_option(id, value);
        update_rdt_clos_mask(id, value);
    });

    $(document).on('click', "button:contains('+')", function() {
        if($(this).text() != '+')
            return;
        var curr_item_id = $(this).attr('id');
        var curr_id = curr_item_id.substr(curr_item_id.lastIndexOf('_')+1);
        var config_item = $(this).parent().parent();
        var config_item_added = config_item.clone();
        var config_vm = config_item.parent();
        var vcpu_index_list = [];
        config_vm.children().each(function(){
            if($(this).find("button:contains('+')").size() > 0) {
                var btn_add_vm_id = $(this).find("button:contains('+')").attr('id');
                vcpu_index_list.push(parseInt(btn_add_vm_id.substr(btn_add_vm_id.lastIndexOf('_')+1)));
            }
        });
        var id_added = 0;
        for (i=0; i<100; i++) {
            if (!vcpu_index_list.includes(i)) {
                id_added = i;
                break
            }
        }
        var id_pre_added = curr_item_id.substr(0, curr_item_id.lastIndexOf('_'));
        config_item_added.find("button:contains('+')").attr('id', id_pre_added+'_'+id_added);
        config_item_added.find("button:contains('-')").attr('id', id_pre_added.replace('add_', 'remove_')+'_'+id_added);
        config_item_added.find("button:contains('-')").prop("disabled", false);
        config_item_added.find("label:first").text("");
        config_item_added.find('.bootstrap-select').replaceWith(function() { return $('select', this); });
        config_item_added.find('.selectpicker').val('default').selectpicker('deselectAll');;
        config_item_added.find('.selectpicker').selectpicker('render');
        config_item_added.insertAfter(config_item);

        if(curr_item_id.indexOf('add_vcpu')>=0) {
            var config_vm = config_item.parent();
            var curr_vcpu_index = vcpu_index_list.indexOf(parseInt(curr_id))
            var vcpu_clos_item = config_vm.find("label:contains('vcpu_clos')").first().parent();
            while(curr_vcpu_index > 0) {
                vcpu_clos_item = vcpu_clos_item.next();
                curr_vcpu_index -= 1;
            }

            var vcpu_clos_item_added = vcpu_clos_item.clone();
            vcpu_clos_item_added.find("label:first").text("");
            vcpu_clos_item_added.find('.bootstrap-select').replaceWith(function() { return $('select', this); });
            vcpu_clos_item_added.find('.selectpicker').val('default').selectpicker('deselectAll');;
            vcpu_clos_item_added.find('.selectpicker').selectpicker('render');
            vcpu_clos_item_added.insertAfter(vcpu_clos_item);
        }
    });

    $(document).on('click', "button:contains('-')", function() {
        if($(this).text() != '-')
            return;
        var config_item = $(this).parent().parent();
        var curr_item_id = $(this).attr('id');
        if(curr_item_id.indexOf('remove_vcpu')>=0) {
            var config_vm = config_item.parent();
            var vcpu_index_list = [];
            config_vm.children().each(function(){
                if($(this).find("button:contains('+')").size() > 0) {
                    var btn_del_vm_id = $(this).find("button:contains('+')").attr('id');
                    vcpu_index_list.push(parseInt(btn_del_vm_id.substr(btn_del_vm_id.lastIndexOf('_')+1)));
                }
            });
            var curr_item_id = $(this).attr('id');
            var curr_id = parseInt(curr_item_id.substr(curr_item_id.lastIndexOf('_')+1));
            curr_vcpu_index = vcpu_index_list.indexOf(curr_id);

            var vcpu_clos_item = config_vm.find("label:contains('vcpu_clos')").first().parent();
            while(curr_vcpu_index > 0) {
                vcpu_clos_item = vcpu_clos_item.next();
                curr_vcpu_index -= 1;
            }
            vcpu_clos_item.remove();
        }
        config_item.remove();
    });

    $('#remove_vm_kata').on('click', function() {
        if(confirm("Do you want to remove the VM?")) {
            save_scenario("remove_vm_kata");
        }
    });

    $('#add_vm_kata').on('click', function() {
        if(confirm("Do you want to add the Kata VM based on generic config?")) {
            save_scenario("add_vm_kata");
        }
    });
})


$(window).load(function () {
　　$("select#scenario_name").change();
});


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


function update_vcpu_clos_option(id, value) {
    if(value == 'y') {
        $("select[ID$='clos,vcpu_clos']").each(function(){
            len = $(this).find('option').length;
            option = $(this).find('option').first();
            for(i=0; i<len; i++){
                if(i>(len-1)/2){
                   option.attr('disabled','disabled');
                }
                option = option.next();
            }
            $(this).selectpicker('render');
        });
    } else {
        $("select[ID$='clos,vcpu_clos']").each(function(){
            len = $(this).find('option').length;
            option = $(this).find('option').first();
            for(i=0; i<len; i++){
                if(i>(len-1)/2){
                   option.removeAttr('disabled');
                }
                option = option.next();
            }
            $(this).selectpicker('render');
        });
    }
}

function update_rdt_clos_mask(id, value) {
    $.ajax({
        type : "POST",
        contentType: "application/json;charset=UTF-8",
        url : "../get_num_of_rdt_res_entries",
        data : JSON.stringify({'cdp_enabled': value}),
        success : function(result) {
            console.log(result);
            num_clos_mask = result.num_clos_mask;
            num_mba_delay = result.num_mba_delay;
            clos_mask_entries = [null];
            index = 0;
            $("input[ID$='hv,FEATURES,RDT,CLOS_MASK']").each(function(){
                index += 1;
                if(index<=num_clos_mask) {
                    clos_mask_entries[0] = $(this).parent().parent();
                }
                if(index>num_clos_mask) {
                    clos_mask_entries.push($(this).parent().parent());
                }
            });
            if(index<=num_clos_mask) {
                last_clos_mask_entry = clos_mask_entries[0];
                for(i=0; i<num_clos_mask-index; i++) {
                    clos_mask_entry_added = last_clos_mask_entry.clone();
                    clos_mask_entry_added.insertAfter(last_clos_mask_entry);
                }
            } else {
                for(i=clos_mask_entries.length-1; i>0; i--) {
                    clos_mask_entries[i].remove();
                }
            }
            mba_delay_entries = [null];
            index = 0;
            $("input[ID$='hv,FEATURES,RDT,MBA_DELAY']").each(function(){
                index += 1;
                if(index<=num_mba_delay) {
                    mba_delay_entries[0] = $(this).parent().parent();
                }
                if(index>num_mba_delay) {
                    mba_delay_entries.push($(this).parent().parent());
                }
            });
            if(index<=num_mba_delay) {
                last_mba_delay_entry = mba_delay_entries[0];
                for(i=0; i<num_mba_delay-index; i++) {
                    mba_delay_entry_added = last_mba_delay_entry.clone();
                    mba_delay_entry_added.insertAfter(last_mba_delay_entry);
                }
            } else {
                for(i=mba_delay_entries.length-1; i>0; i--) {
                    mba_delay_entries[i].remove();
                }
            }
        },
        error : function(e){
            console.log(e.status);
            console.log(e.responseText);
            alert(e.responseText);
        }
    });
}

function create_setting(type, default_name, name, mode){
    var board_info = $("text#board_type").text();
    if (board_info==null || board_info=='') {
        alert("Please select one board info before this operation.");
        return;
    }

    create_config = {
        board_info: board_info,
        type: type,
        default_name: default_name,
        create_name: name,
        mode: mode
    }

    $.ajax({
        type : "POST",
        contentType: "application/json;charset=UTF-8",
        url : "../check_setting_exist",
        data : JSON.stringify(create_config),
        success : function(result) {
            exist = result.exist
            create_flag = true
            if(exist == "yes") {
                overwirte_confirm_message = 'Setting name: ' + create_config['create_name'] + ' existed in ' +
                    'acrn-hypervisor/misc/vm_configs/xmls/config-xmls/'+board_info+'/user_defined/.\n'+
                    'Do you want to overwrite it?\nClick OK to overwrite it; click Cancel to rename it.'
                if(!confirm(overwirte_confirm_message)) {
                    create_flag = false
                }
            }
            if(create_flag == true) {
                $.ajax({
                    type : "POST",
                    contentType: "application/json;charset=UTF-8",
                    url : "../create_setting",
                    data : JSON.stringify(create_config),
                    success : function(result) {
                        console.log(result);
                        status = result.status
                        setting = result.setting
                        error_list = result.error_list
                        if (status == 'success' && (JSON.stringify(error_list)=='{}' || JSON.stringify(error_list)=='null')) {
                            alert('create a new setting successfully.');
                        } else {
                            alert('create a new setting failed. \nError list:\n'+JSON.stringify(error_list));
                        }
                        var href = window.location.href
                        if(href.endsWith("/scenario") || href.endsWith("/launch")) {
                            window.location = type + "/" + setting;
                        } else {
                            window.location = "../" + type + "/" + setting;
                        }
                    },
                    error : function(e){
                        $("#create_modal").modal("hide");
                        $("#load_scenario_modal").modal("hide");
                        $("#load_launch_modal").modal("hide");
                        console.log(e.status);
                        console.log(e.responseText);
                        alert(e.status+'\n'+e.responseText);
                    }
                });
            }
        },
        error : function(e){
            console.log(e.status);
            console.log(e.responseText);
            alert(e.status+'\n'+e.responseText);
        }
    });

}


function save_scenario(generator=null){
    var board_info = $("text#board_type").text();
    if (board_info==null || board_info=='') {
        alert("Please select one board info before this operation.");
        return;
    }

    scenario_config = {
        old_scenario_name: $("#old_scenario_name").text(),
        generator: generator
    }

    if(generator!=null && generator.indexOf('add_vm:')==0) {
        scenario_config['new_scenario_name'] = $("#new_scenario_name2").val()
    } else if(generator!=null && generator.indexOf('remove_vm:')==0) {
        scenario_config['new_scenario_name'] = $("#old_scenario_name").text()
    } else {
        scenario_config['new_scenario_name'] = $("#new_scenario_name").val()
    }

    $("input").each(function(){
        var id = $(this).attr('id');
        var value = $(this).val();
        if(id.indexOf('CLOS_MASK')>=0 || id.indexOf('MBA_DELAY')>=0) {
            if(id in scenario_config) {
                scenario_config[id].push(value);
            } else {
                scenario_config[id] = [value];
            }
        } else if(id!='new_scenario_name' && id!='new_scenario_name2' && id!='board_info_file' && id!='board_info_upload'
            && id!='scenario_file' && id!='create_name' && id!='load_scenario_name2' && id!='load_launch_name2'
            && id!='src_path') {
            scenario_config[id] = value;
        }
    })

    $("textarea").each(function(){
        var id = $(this).attr('id');
        var value = $(this).val();
        scenario_config[id] = value;
    })

    $("select").each(function(){
        var id = $(this).attr('id');
        var value = $(this).val();
        if(id.indexOf('pcpu_id')>=0 || id.indexOf('pci_dev')>=0 || id.indexOf('vcpu_clos')>=0) {
            if(id in scenario_config) {
                scenario_config[id].push(value);
            } else {
                scenario_config[id] = [value];
            }
        } else if(id!='board_info' && id!='load_scenario_name' && id!='load_launch_name') {
            scenario_config[id] = value;
        }
    })

    $.ajax({
        type : "POST",
        contentType: "application/json;charset=UTF-8",
        url : "../check_setting_exist",
        data : JSON.stringify(scenario_config),
        success : function(result) {
            exist = result.exist
            create_flag = true
            if(exist == "yes") {
                overwirte_confirm_message = 'Setting name: ' + scenario_config['create_name'] + ' existed in ' +
                    'acrn-hypervisor/misc/vm_configs/xmls/config-xmls/'+board_info+'/user_defined/.\n'+
                    'Do you want to overwrite it?\nClick OK to overwrite it; click Cancel to rename it.'
                if(!confirm(overwirte_confirm_message)) {
                    create_flag = false
                }
            }
            if(create_flag == true) {
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
                            validate_message = 'Scenario setting saved successfully with name: '
                                +file_name+'\ninto acrn-hypervisor/misc/vm_configs/xmls/config-xmls/'+board_info+'/user_defined/.'
                            if(result.rename==true) {
                                validate_message = 'Scenario setting existed, saved successfully with a new name: '
                                    +file_name+'\ninto acrn-hypervisor/misc/vm_configs/xmls/config-xmls/'+board_info+'/user_defined/.';
                            }
                            if(generator=="generate_config_src") {
                                var src_path = $("input#src_path").val();
                                generate_flag = true;
                                if(src_path == null || src_path == '') {
                                    overwirte_confirm_message = 'The Source Path for configuration files is not set.\n' +
                                        'Do you want to generate them into the default path: acrn-hypervisor/misc/vm_configs/board/ and acrn-hypervisor/misc/vm_configs/scenarios/,\n'+
                                        'and overwrite the old ones?\nClick OK to overwrite them; click Cancel to edit the Source Path.'
                                    if(!confirm(overwirte_confirm_message)) {
                                        generate_flag = false
                                    }
                                }
                                if(generate_flag) {
                                generator_config = {
                                    type: generator,
                                    board_info: $("select#board_info").val(),
                                    board_setting: "board_setting",
                                    scenario_setting: file_name,
                                    src_path: src_path,
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
                                        if (status == 'success' && (JSON.stringify(error_list)=='{}' || JSON.stringify(error_list)=='null')) {
                                            if(src_path==null || src_path=='') {
                                                alert(generator+' successfully into acrn-hypervisor/misc/vm_configs/board/ and acrn-hypervisor/misc/vm_configs/scenarios/ ');
                                            } else {
                                                alert(generator+' successfully into '+src_path);
                                            }
                                        } else {
                                            alert(generator+' failed. \nError list:\n'+JSON.stringify(error_list));
                                        }
                                        window.location = "./" + file_name;
                                    },
                                    error : function(e){
                                        console.log(e.status);
                                        console.log(e.responseText);
                                        alert(e.status+'\n'+e.responseText);
                                    }
                                });
                                }
                            } else {
                                alert(validate_message);
                                window.location = "./" + file_name;
                            }
                        }
                        else {
                            $("#save_modal").modal("hide");
                            alert(JSON.stringify(error_list));
                        }
                    },
                    error : function(e){
                        $("#save_modal").modal("hide");
                        console.log(e.status);
                        console.log(e.responseText);
                        alert(e.status+'\n'+e.responseText);
                    }
                });
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
    var board_info = $("text#board_type").text();
    var scenario_name = $("select#scenario_name").val();
    if (board_info==null || board_info=='' || scenario_name==null || scenario_name=='') {
        alert("Please select one board and scenario before this operation.");
        return;
    }

    launch_config = {
        old_launch_name: $("#old_launch_name").text(),
        scenario_name: scenario_name,
        generator: generator
    }

    if(generator!=null && generator.indexOf('add_vm:')==0) {
        launch_config['new_launch_name'] = $("#new_launch_name2").val()
    } else if(generator!=null && generator.indexOf('remove_vm:')==0) {
        launch_config['new_launch_name'] = $("#old_launch_name").text()
    } else {
        launch_config['new_launch_name'] = $("#new_launch_name").val()
    }

    $("input").each(function(){
        var id = $(this).attr('id');
        var value = $(this).val();

        if(id.indexOf('virtio_devices,network')>=0 || id.indexOf('virtio_devices,block')>=0
            || id.indexOf('virtio_devices,input')>=0) {
            if(id in launch_config) {
                launch_config[id].push(value);
            } else {
                launch_config[id] = [value];
            }
        } else if(id!='new_launch_name' && id!='new_launch_name2' && id!='board_info_file' && id!='board_info_upload'
            && id!="launch_file" && id!='create_name'  && id!='load_scenario_name2' && id!='load_launch_name2'
            && id!='src_path') {
            launch_config[id] = value;
        }
    })

    $("select").each(function(){
        var id = $(this).attr('id');
        var value = $(this).val();
        if(id.indexOf('pcpu_id')>=0 || id.indexOf('pci_dev')>=0) {
            if(id in launch_config) {
                launch_config[id].push(value);
            } else {
                launch_config[id] = [value];
            }
        } else if(id!='board_info' && id!='load_scenario_name' && id!='load_launch_name') {
            launch_config[id] = value;
        }
    })

    $("textarea").each(function(){
        var id = $(this).attr('id');
        var value = $(this).val();
        launch_config[id] = value;
    })

    $.ajax({
        type : "POST",
        contentType: "application/json;charset=UTF-8",
        url : "../check_setting_exist",
        data : JSON.stringify(launch_config),
        success : function(result) {
            exist = result.exist
            create_flag = true
            if(exist == "yes") {
                overwirte_confirm_message = 'Setting name: ' + launch_config['create_name'] + ' existed in ' +
                    'acrn-hypervisor/misc/vm_configs/xmls/config-xmls/'+board_info+'/user_defined/.\n'+
                    'Do you want to overwrite it?\nClick OK to overwrite it; click Cancel to rename it.'
                if(!confirm(overwirte_confirm_message)) {
                    create_flag = false
                }
            }
            if(create_flag == true) {
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
                            validate_message = 'Launch setting saved successfully with name: '
                                +file_name+'\nto acrn-hypervisor/misc/vm_configs/xmls/config-xmls/'+board_info+'/user_defined/.'
                            if(result.rename==true) {
                                validate_message = 'Launch setting existed, saved successfully with a new name: '
                                    +file_name+'\nto acrn-hypervisor/misc/vm_configs/xmls/config-xmls/'+board_info+'/user_defined/.';
                            }
                            if(generator == 'generate_launch_script') {
                                var src_path = $("input#src_path").val();
                                generate_flag = true;
                                if(src_path == null || src_path == '') {
                                    overwirte_confirm_message = 'The Source Path for launch scripts is not set.\n' +
                                        'Do you want to generate them into the default path: misc/vm_configs/xmls/config-xmls/'+board_info+'/output/,\n'+
                                        'and overwrite the old ones?\nClick OK to overwrite them; click Cancel to edit the Source Path.'
                                    if(!confirm(overwirte_confirm_message)) {
                                        generate_flag = false
                                    }
                                }
                                if(generate_flag) {
                                generator_config = {
                                    type: generator,
                                    board_info: $("select#board_info").val(),
                                    board_setting: "board_setting",
                                    scenario_setting: $("select#scenario_name").val(),
                                    launch_setting: file_name,
                                    src_path: src_path,
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
                                        if (status == 'success' && (JSON.stringify(error_list)=='{}' || JSON.stringify(error_list)=='null')) {
                                            if(src_path==null || src_path==='') {
                                                alert(generator+' successfully into '+
                                                      'acrn-hypervisor/misc/vm_configs/xmls/config-xmls/'+board_info+'/output/.');
                                            } else {
                                                alert(generator+' successfully into '+src_path);
                                            }
                                        } else {
                                            alert(generator+' failed. \nError list:\n'+JSON.stringify(error_list));
                                        }
                                        window.location = "./" + file_name;
                                    },
                                    error : function(e){
                                        console.log(e.status);
                                        console.log(e.responseText);
                                        alert(e.status+'\n'+e.responseText);
                                    }
                                });
                                }
                            } else {
                                alert(validate_message);
                                window.location = "./" + file_name;
                            }
                        }
                        else {
                            $("#save_modal").modal("hide");
                            alert(JSON.stringify(error_list));
                        }
                    },
                    error : function(e){
                        $("#save_modal").modal("hide");
                        console.log(e.status);
                        console.log(e.responseText);
                        alert(e.status+'\n'+e.responseText);
                    }
                });
            }
        },
        error : function(e){
            console.log(e.status);
            console.log(e.responseText);
            alert(e.status+'\n'+e.responseText);
        }
    });
}
