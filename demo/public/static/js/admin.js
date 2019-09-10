layui.config({base: '/static/js/'}).define(['layer', 'form', 'laytpl', 'element', 'laypage'], function(exports) {
    var layer = layui.layer,
        form = layui.form,
        laytpl = layui.laytpl,
        element = layui.element,
        laypage = layui.laypage,
        $ = layui.$;


    tableCheck = {
        init:function  () {
            $(document).on('click', '.layui-form-checkbox', function () {
                var check = $(this).not('.status-btn');
                if(check.hasClass('layui-form-checked')){
                    check.removeClass('layui-form-checked');
                    if(check.hasClass('header')){
                        $(".layui-form-checkbox").not('.status-btn').removeClass('layui-form-checked');
                    }
                }else{
                    check.addClass('layui-form-checked');
                    if(check.hasClass('header')){
                        $(".layui-form-checkbox").not('.status-btn').addClass('layui-form-checked');
                    }
                }
            });
        },
        getData:function  () {
            var obj = $(".layui-form-checked").not('.header').not('.status-btn');
            var arr=[];
            obj.each(function(index, el) {
                arr.push(obj.eq(index).attr('data-id'));
            });
            return arr;
        }
    }

    //开启表格多选
    tableCheck.init();
      

    $(document).on('click', '.container .left_open i', function () {
        if($('.left-nav').css('left')=='0px'){
            $('.left-nav').animate({left: '-221px'}, 100);
            $('.page-content').animate({left: '0px'}, 100);
            $('.page-content-bg').hide();
        }else{
            $('.left-nav').animate({left: '0px'}, 100);
            $('.page-content').animate({left: '221px'}, 100);
            if($(window).width()<768){
                $('.page-content-bg').show();
            }
        }
    });
    
    $(document).on('click', '.page-content-bg', function () {
        $('.left-nav').animate({left: '-221px'}, 100);
        $('.page-content').animate({left: '0px'}, 100);
        $(this).hide();
    });

    $(document).on('click', '.layui-tab-close', function () {
        $('.layui-tab-title li').eq(0).find('i').remove();
    });

    $(document).on('click', '.left-nav #nav li', function () {
        if($(this).children('.sub-menu').length){
            if($(this).hasClass('open')){
                $(this).removeClass('open');
                $(this).find('.nav_right').html('&#xe697;');
                $(this).children('.sub-menu').stop().slideUp();
                $(this).siblings().children('.sub-menu').slideUp();
            }else{
                $(this).addClass('open');
                $(this).children('a').find('.nav_right').html('&#xe6a6;');
                $(this).children('.sub-menu').stop().slideDown();
                $(this).siblings().children('.sub-menu').stop().slideUp();
                $(this).siblings().find('.nav_right').html('&#xe697;');
                $(this).siblings().removeClass('open');
            }
        }else{
            var url = $(this).children('a').attr('_href');
            var title = $(this).find('cite').html();
            var index  = $('.left-nav #nav li').index($(this));
            for (var i = 0; i <$('.x-iframe').length; i++) {
                if($('.x-iframe').eq(i).attr('tab-id')==index+1){
                    tab.tabChange(index+1);
                    event.stopPropagation();
                    return;
                }
            };
            tab.tabAdd(title,url,index+1);
            tab.tabChange(index+1);
        }
        event.stopPropagation();
    });
    
    //监听提交
    form.on('submit(geneForm)', function(data){
        var _obj = $(this);
        $(_obj).attr("disabled",true);
        var url = $(this).attr('data-url');
        layer.load();
        $.ajax({  
            type: 'POST',  
            url: url,
            data: data.field,  
            dataType : "json",  
            success: function(result) {
                layer.closeAll('loading');
                var type = (result.code >= 2000 && result.code <3000)  ? 1 : 0;
                layer.msg(result.msg, {icon: type, time: 3000}, function(){
                    var index = parent.layer.getFrameIndex(window.name);
                    if (type && typeof(index) != undefined) {
                        parent.layer.close(index);
                        parent.location.reload();
                    }
                    $(_obj).attr("disabled",false);   
                });
            }  
        });     
        return false;
    });
    
	//顶部批量删除
    $(document).on('click', '.del-all', function () {
        var _obj = $(this);
        $(_obj).attr("disabled",true);
        var url= $(_obj).attr('data-url');
        layer.confirm('您确定要删除选中项吗？', {icon: 3, title:'提示'}, function(index){
            layer.close(index);
            layer.load();
            $.ajax({  
                type: 'POST',  
                url: url,
                data: {"id":tableCheck.getData()},  
                dataType : "json",  
                success: function(result) {  
                    layer.closeAll('loading');
                    var type = (result.code >= 2000 && result.code <3000)  ? 1 : 0;
                    layer.msg(result.msg, {icon: type, time: 2000}, function(){
                        if (type) {
                            $(".layui-form-checked").not('.header').not('.status-btn').parents('tr').remove();
                        }
                    });
                }  
            });  
        });
        $(_obj).attr("disabled", false);
	return false;
    });
    
	//列表刷新
    $(document).on('click', '.reload-btn', function () {
        location.reload();
    });
	//列表添加
    $(document).on('click', '.add-btn', function () {
        var That = $(this);
        var title = That.attr('data-title');
        var url = That.attr('data-url');
        adminShow(title, url, 720, 530);
        return false;
    });
	//列表编辑
    $(document).on('click', '.edit-btn', function () {
        var That = $(this);
        var title = That.attr('data-title');
        var url = That.attr('data-url');
        adminShow(title, url, 720, 530);
        return false;
    });
    //列表删除
    $(document).on('click', '.del-btn', function () {
            var obj = $(this);
            $(obj).attr("disabled",true);
            var url = obj.attr('data-url');
            layer.confirm('您确定要进行删除吗？', {icon: 3, title:'提示'}, function(index){
                layer.close(index);
                layer.load();
                $.ajax({  
                    type: 'GET',  
                    url: url,
                    data: {},  
                    dataType : "json",  
                    success: function(result) {  
                        layer.closeAll('loading');
                        var type = (result.code >= 2000 && result.code <3000)  ? 1 : 0;
                        layer.msg(result.msg, {icon: type, time: 2000}, function(){
                            if (type) {
                                $(obj).parents("tr").remove();
                            }
                        });
                    }  
                });
            });
            $(obj).attr("disabled",false);
            return false;
    });
    //列表跳转
    $(document).on('click', '.go-btn', function () {
        var url = $(this).attr('data-url');
        window.location.href = url;
        return false;
    });
    //列表状态修改
    $(document).on('click', '.status-btn', function () {
        var obj = $(this);
        $(obj).attr("disabled",true);
        var url = obj.attr('data-url');
        layer.load();
        $.ajax({  
            type: 'GET',  
            url: url,
            data: {},  
            dataType : "json",  
            success: function(result) {  
                layer.closeAll('loading');
                var type = (result.code >= 2000 && result.code <3000)  ? 1 : 0;
                layer.msg(result.msg, {icon: type, time: 2000}, function(){
                    if (type && obj.hasClass('layui-form-checked')) {
                        obj.removeClass('layui-form-checked');
                    } else {
                        obj.addClass('layui-form-checked');
                    }
                    $(obj).attr("disabled", false);
                });
            }  
        });
        return false;
    });
    //ajax处理
    $(document).on('click', '.ajax-btn', function () {
        var obj = $(this);
        $(obj).attr("disabled",true);
        var url = obj.attr('data-url');
        layer.load();
        $.ajax({  
            type: 'GET',  
            url: url,
            data: {},  
            dataType : "json",  
            success: function(result) {  
                layer.closeAll('loading');
                var type = (result.code >= 2000 && result.code <3000)  ? 1 : 0;
                layer.msg(result.msg, {icon: type, time: 2000}, function(){
                    $(obj).attr("disabled", false);
                });
            }  
        });
        return false;
    });
    //退出系统
    $(document).on('click', '.exit-btn', function () {
        var obj = $(this);
        $(obj).attr("disabled",true);
        var url = obj.attr('data-url');
        layer.confirm('您确定要退出系统吗？', {icon: 3, title:'提示'}, function(index){
            layer.close(index);;
            layer.load();
            $.ajax({  
                type: 'GET',  
                url: url,
                data: {},  
                dataType : "json",  
                success: function(result) {  
                    layer.closeAll('loading');
                    var type = (result.code >= 2000 && result.code <3000)  ? 1 : 0;
                    layer.msg(result.msg, {icon: type, time: 2000}, function(){
                        location.reload();
                    });
                }  
            });
        });
        $(obj).attr("disabled", false);
        return false;
    });
});

/*
    弹出层,参数解释：
    title   标题
    url     请求的url
    id      需要操作的数据id
    w       弹出层宽度（缺省调默认值）
    h       弹出层高度（缺省调默认值）
*/
function adminShow(title, url, w, h){
    if (title == null || title == '') {
        title=false;
    };
    if (url == null || url == '') {
        url="404.html";
    };
    if (w == null || w == '') {
        w = ($(window).width()*0.9);
    };
    if (h == null || h == '') {
        h = ($(window).height() - 50);
    };
    layer.open({
        type: 2,
        area: [w+'px', h +'px'],
        fix: false, //不固定
        maxmin: true,
        shadeClose: true,
        closeBtn:1,
        shade:0.4,
        title: title,
        content: url
    });
}

function iniPage(page, count, pageSize) {
    pageSize = typeof(pageSize) == undefined ? 10 : pageSize;
    layui.use('laypage', function(){
        var laypage = layui.laypage;
        laypage.render({
        elem: 'page',
        count: count,
        curr: page,
        limit: pageSize,
        layout: ['count', 'prev', 'page', 'next', 'limit', 'refresh', 'skip'],
        jump: function(obj, first){
                if(!first){
                    var url = window.location.href;
                    url = jsUrlHelper.putUrlParam(url,'page',obj.curr);
                    url = jsUrlHelper.putUrlParam(url,'limit',obj.limit);
                    window.location.href = url;
                }
            }
        });
    });
}

function initForm(name, params) {
    layui.use(['form', 'layedit', 'laydate'], function(){
      var form = layui.form
      ,layer = layui.layer
      ,layedit = layui.layedit
      ,laydate = layui.laydate;
      
      form.val(name, params);
    });
}

function postPage(url, params) {
    var temp_form = document.createElement("form");
    temp_form.action = URL;
    temp_form.target = "_self";
    temp_form.method = "POST";
    temp_form.style.display = "none";
    for (var item in params) {
        var opt = document.createElement("textarea");
        opt.name = params[item].name;
        opt.value = params[item].value;
        temp_form.appendChild(opt);
    }
    document.body.appendChild(temp_form);
    temp_form.submit();
}

var jsUrlHelper = {
    getUrlParam : function(url, ref) {
        var str = "";
        if (url.indexOf(ref) == -1)
            return "";
        str = url.substr(url.indexOf('?') + 1);
        arr = str.split('&');
        for (i in arr) {
            var paired = arr[i].split('=');
            if (paired[0] == ref) {
                return paired[1];
            }
        }
        return "";
    },
    putUrlParam : function(url, ref, value) {
        if (url.indexOf('?') == -1)
            return url + "?" + ref + "=" + value;
        if (url.indexOf(ref) == -1)
            return url + "&" + ref + "=" + value;
        var arr_url = url.split('?');
        var base = arr_url[0];
        var arr_param = arr_url[1].split('&');
        for (i = 0; i < arr_param.length; i++) {
            var paired = arr_param[i].split('=');
            if (paired[0] == ref) {
                paired[1] = value;
                arr_param[i] = paired.join('=');
                break;
            }
        }
        return base + "?" + arr_param.join('&');
    },
    delUrlParam : function(url, ref) {
        if (url.indexOf(ref) == -1)
            return url;
        var arr_url = url.split('?');
        var base = arr_url[0];
        var arr_param = arr_url[1].split('&');
        var index = -1;
        for (i = 0; i < arr_param.length; i++) {
            var paired = arr_param[i].split('=');
            if (paired[0] == ref) {
                index = i;
                break;
            }
        }
        if (index == -1) {
            return url;
        } else {
            arr_param.splice(index, 1);
            return base + "?" + arr_param.join('&');
        }
    }
};

Array.prototype.each = function(f) {
    for (var i = 0; i < this.length; i++)
        f(this[i], i, this);
}

function f(a, b) {
    var sumRow = function(row) {
        return Number(row.cells[1].innerHTML) + Number(row.cells[2].innerHTML)
    };
    return sumRow(a) > sumRow(b) ? 1 : -1;
}

function varPush(arrayLike) {
    for (var i = 0, ret = []; i < arrayLike.length; i++)
        ret.push(arrayLike[i]);
    return ret;
}

Function.prototype.bind = function() {
    var __method = this, args = varPush(arguments), object = args.shift();
    return function() {
        return __method.apply(object, args.concat(varPush(arguments)));
    }
}

function CTable(id, rows, target_obj, arr) {
    this.tbl = typeof (id) == "string" ? document.getElementById(id) : id;
    this.add_obj = typeof (target_obj) == "string" ? document.getElementById(target_obj) : target_obj;
    this.num = 0;
    this.arr = typeof (arr) == "object" ? arr : ["input", "select", "textarea", "img", "label","a"];
    if (rows && /^\d+$/.test(rows)) {
        this.addrows(rows);
    }
}

CTable.prototype = {
    addrows: function(n) {
        new Array(n).each(this.add.bind(this))
    },
    rendor: function(n) {
        layui.use(['form'], function(){
            var form = layui.form
            ,layer = layui.layer
            form.render('select'); 
        });
    },  
    add: function() {
        var self = this;
        this.num++;
        var tr = self.tbl.insertRow(-1);
        var content = '';
        var innerhtml = this.add_obj.innerHTML;
        if (innerhtml) {
            content = innerhtml.replace(/{n}/g, this.num).replace(/keyNum/g, this.num);
        }
        tr.id = this.num;
        tr.innerHTML = content;
        this.rendor();
    },
    addData: function(data) {
        var self = this;
        this.num++;
        var tr = self.tbl.insertRow(-1);
        var content = '';
        var innerhtml = this.add_obj.innerHTML;
        if (innerhtml) {
            content = innerhtml;
            for (var i = 0; i < data.length; i++)
            {
                var reg = new RegExp("{" + data[i].key + "}", "g");
                content = content.replace(reg, data[i].value);
            }
            content = content.replace(/{n}/g, this.num).replace(/keyNum/g, this.num);
        }
        tr.id = this.num;
        tr.innerHTML = content;
    },
    del: function(obj_click) {
        var self = this;
        var tr_click = obj_click.parentNode.parentNode;
        varPush(self.tbl.rows).each(function(tr) {
            if (tr == tr_click)
                tr.parentNode.removeChild(tr);
        });
    },
    up: function(obj_click) {
        var self = this;
        var tr_click = obj_click.parentNode.parentNode;
        var upOne = function(tr) {
            if (tr.rowIndex > 1) {
                self.swapValue(tr, self.tbl.rows[tr.rowIndex - 2]);
            }
        }
        varPush(self.tbl.rows).each(function(tr) {
            if (tr == tr_click)
                upOne(tr);
        });
    },
    down: function(obj_click) {
        var self = this;
        var tr_click = obj_click.parentNode.parentNode;
        var downOne = function(tr) {
            if (tr.rowIndex < self.tbl.rows.length) {
                self.swapValue(tr, self.tbl.rows[tr.rowIndex]);
            }
        }
        varPush(self.tbl.rows).each(function(tr) {
            if (tr == tr_click)
                downOne(tr);
        });
    },
    highlight: function() {
        var self = this;
        var tr_click = obj_click.parentNode.parentNode;
        self.restoreBgColor();
    },
    swapValue: function(tr1, tr2) {
        var arr = this.arr;
        var a, b, c;
        for (i = 0; i < arr.length; i++) {
            var a = tr1.getElementsByTagName(arr[i]);
            var b = tr2.getElementsByTagName(arr[i]);
            for (j = 0; j < a.length; j++) {
                switch (arr[i]) {
                    case 'input':
                        if (a[j].type == "radio") {
                            c = a[j].checked;
                            b[j].checked == true ? a[j].checked = true : a[j].checked = false;
                            c == true ? b[j].checked = true : b[j].checked = false;
                        } else if (a[j].type == "checkbox") {
                            c = a[j].checked;
                            b[j].checked == true ? a[j].checked = true : a[j].checked = false;
                            c == true ? b[j].checked = true : b[j].checked = false;
                        } else {
                            this.swapProp(a[j],b[j],"value");
                        }
                        this.swapProp(a[j],b[j],"name");
                        this.swapProp(a[j],b[j],"id");
                        break;
                    case 'select':
                        this.swapProp(a[j],b[j],"value");
                        this.swapProp(a[j],b[j],"name");
                        this.swapProp(a[j],b[j],"id");
                        break;
                    case 'textarea':
                        this.swapProp(a[j],b[j],"value");
                        this.swapProp(a[j],b[j],"name");
                        this.swapProp(a[j],b[j],"id");
                        break;
                    case 'img':
                        c = a[j].src;
                        a[j].src = b[j].src;
                        b[j].src = c;
                        break;
                    case 'label':
                        this.swapProp(a[j],b[j],"innerHTML");
                        break;
                    case 'a':
                        this.swapProp(a[j],b[j],"id");
                        break;
                }
            }

        }
        var tr1_index = tr1.id;
        var tr2_index = tr2.id;
        var editor_c = document.getElementById('ueditor' + tr1_index);
        if (editor_c) {
            var ueditorA = UE.getEditor('ueditor' + tr1_index);
            var ueditorB = UE.getEditor('ueditor' + tr2_index);
            if (ueditorA && ueditorB) {
                var bodyA = ueditorA.getContent();
                var bodyB = ueditorB.getContent();
                ueditorA.setContent(bodyB);
                ueditorB.setContent(bodyA);
            }
        }
    },
    restoreBgColor: function(tr) {
        tr.style.backgroundColor = "#ffffff";
    },
    setBgColor: function(tr) {
        tr.style.backgroundColor = "#c0c0c0";
    },
    swapProp: function(one,two,types) {
        try{
            var c = one[types];
            one[types] = two[types];
            two[types] = c;  
        }catch(err){
            
        }
    }
}
