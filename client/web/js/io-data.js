
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

var initLists = [
  { value: '{"cmd":"mdb.get", "key":"SH600600.inout_day"}' },
  { value: '{"cmd":"mdb.get", "key":"SH600600.inout_min"}' },
  { value: '{"cmd":"mdb.get", "key":"SH600600.inout_wave"}' },
  { value: '{"cmd":"mdb.get", "key":"SH600600.inout_space"}' },
  { value: '{"cmd":"mdb.get", "key":"SH600600.inout_snapshot"}' },

  { value: '{"cmd":"mdb.get", "key":"SH600600.stk_day"}' },
  { value: '{"cmd":"mdb.get", "key":"SH600600.stk_min"}' },

  { value: '{"cmd":"mdb.get", "key":"SH600600.stk_right"}' },
  { value: '{"cmd":"mdb.get", "key":"SH600600.stk_finance"}' },
  { value: '{"cmd":"nowdb.sub", "key":"sh600601.stk_snapshot", "val":{"date":20200730,"format":"array"}}' },
  { value: '{"cmd":"nowdb.get", "key":"sh600601.stk_snapshot", "val":{"date":20200730,"format":"array"}}' },
  { value: '{"cmd":"show"}' },
  { value: '{"cmd":"auth", "key":"guest", "val":"guest1234"}' },
  { value: '{"cmd":"save"}' },
  { value: '{"cmd":"pack"}' },
]; 

function loadListData(){
  var lists = initLists;
  if (localStorage.length < 1)
  {
    return lists;
  }
  lists = localStorage.getItem('sisdb-commands');
  var array = JSON.parse(lists);
  // console.log(lists, array.length, JSON.parse(lists));
  lists = [];
  for (var index = 0; index < array.length; index++) {
    lists.push(array[index]); 
  }
  return lists;
}

function saveListData(comstr){
  var count = initLists.length;
  for (var i = 0; i < count; i++) 
  {
	  if (initLists[i].value == comstr)
	  {
		  return ;
	  }
  }
  initLists.push({value:comstr});
  var jstr = JSON.stringify(initLists);
  localStorage.setItem('sisdb-commands', jstr);
}

$(function(){
  // setup autocomplete function pulling from currencies[] array
  $('#send-cmd').autocomplete({
    lookup: loadListData(),
    // onSelect: function (suggestion) {
    //   var thehtml = '<strong>Currency Name:</strong> ' + suggestion.value + ' <br> <strong>Symbol:</strong> ' + suggestion.data;
    //   $('#outputcontent').html(thehtml);
    //   console.log(suggestion.value);
    // }
  });
});