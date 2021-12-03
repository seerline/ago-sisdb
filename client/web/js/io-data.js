
// function showListData(){
//   var t = document.getElementById("tbBody");
//   console.log("删除前，数据数量：" + t.rows.length);
//   console.log("$ = " + $);
//   $("#tbBody").empty();
//   $("#tbBody").append(createTitleRow());
//   var objList = new Array();
//   var count = localStorage.length;
//   if(count>0){
//       for (var i = 0; i < count; i++) {
//           var keyName = localStorage.key(i);
//           var str = localStorage.getItem(keyName);
//           var task = JSON.parse(str);
//           if(task != null && task != undefined && task.isDelete == false){
//               objList[objList.length] = task;
//           }
//       }
//   }
//   if(objList.length > 0){
//       for (var i = 0; i < objList.length; i++) {
//           var task = objList[i];
//           var rowStr = createTr(task);
//            console.log("rowStr: " + rowStr);
//            $("#tbBody").append(rowStr);
//       }
//   }
// }
// localStorage.setItem('key', 'value');
// // 从 localStorage 获取数据
// let data = localStorage.getItem('key');
// // 从 localStorage 删除保存的数据
// localStorage.removeItem('key');
// // 从 localStorage 删除所有保存的数据
// localStorage.clear();

var initLists = {
  sisdb_service : ['after'],
  sisdb_cmd : ['get'],
  sisdb_key : ['sh600600.stk_day'],
  sisdb_ask : ['{"range":{"start":0,"stop":-1}}', '{"where":{"start":19930911,"offset":-1}}']
};

function loadListData(wname){
  var inits = initLists[wname];
  if (localStorage.length < 1)
  {
    return inits;
  }
  var currs = localStorage.getItem(wname);
  if (currs == null)
  {
    return inits;
  }
  // console.log(currs);
  var array = JSON.parse(currs);
  // console.log(currs, array.length, JSON.parse(currs));
  currs = [];
  for (var index = 0; index < array.length; index++) {
    currs.push(array[index]); 
  }
  return currs;
}

function saveListData(wname, comstr){
  var curr_name = wname + '_curr';
  localStorage.setItem(curr_name, comstr);
  if (comstr == undefined || comstr == '')
  {
    return ;
  }
  var count = initLists[wname].length;
  for (var i = 0; i < count; i++) 
  {
	  if (initLists[wname][i] == comstr)
	  {
		  return ;
	  }
  }
  initLists[wname].push(comstr);
  var jstr = JSON.stringify(initLists[wname]);
  localStorage.setItem(wname, jstr);
}

$(function(){
  // setup autocomplete function pulling from currencies[] array
  $('#send-cmd').autocomplete({
    lookup: loadListData('sisdb_cmd'),
    // onSelect: function (suggestion) {
    //   var thehtml = '<strong>Currency Name:</strong> ' + suggestion.value + ' <br> <strong>Symbol:</strong> ' + suggestion.data;
    //   $('#outputcontent').html(thehtml);
    //   console.log(suggestion.value);
    // }
  });
  $('#send-service').autocomplete({
    lookup: loadListData('sisdb_service'),
  });
  $('#send-key').autocomplete({
    lookup: loadListData('sisdb_key'),
  });
  $('#send-ask').autocomplete({
    lookup: loadListData('sisdb_ask'),
  });
  document.getElementById("send-service").value = localStorage.getItem('sisdb_service' + '_curr');;
  document.getElementById("send-cmd").value = localStorage.getItem('sisdb_cmd' + '_curr');;
  document.getElementById("send-key").value = localStorage.getItem('sisdb_key' + '_curr');;
  document.getElementById("send-ask").value = localStorage.getItem('sisdb_ask' + '_curr');;
});