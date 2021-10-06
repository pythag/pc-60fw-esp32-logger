const char root_page[] PROGMEM = R"=====(<!DOCTYPE html>
<html>
  <head>
    <script type="text/javascript" src="https://code.jquery.com/jquery-1.7.1.min.js"></script>
    <script type="text/javascript" src="https://www.gstatic.com/charts/loader.js"></script>
    <script type="text/javascript">
     
      google.charts.load('current', {'packages':['line']});
      google.charts.load('current', {'packages':['table']});

      google.charts.setOnLoadCallback(drawChart);

      function drawChart() {

        var jsonData = $.ajax({
            url: "readings",
            dataType: "json",
            async: false
            }).responseText;
            
        // Create our data table out of JSON data loaded from server.
        var data = new google.visualization.DataTable(jsonData);
        
        var options = {
          height: 500
        };

        var chart = new google.charts.Line(document.getElementById('linechart_material'));
        var table = new google.visualization.Table(document.getElementById('table_div'));
        
        chart.draw(data, google.charts.Line.convertOptions(options));

        table.draw(data, {showRowNumber: true, width: '100%', height: '100%'});
        
      }
    </script>
  </head>
  <body>
    <div id="linechart_material" style="width: 100%; height: 500px"></div>
    <div id="table_div"></div><br>
    Click <a href=/eraseconfirm>HERE</a> to clear all data<br>
  </body>
</html>
)=====";

const char eraseconfirm_page[] PROGMEM = R"=====(<!DOCTYPE html>
<html>
  <body>
    Click <a href=/erase>HERE</a> to confirm clear of all data - no way to undo!<br>
    <a href=/>Cancel</a>
  </body>
</html>
)=====";

const char graph_json_header[] PROGMEM = R"=====({
  "cols": [
    {"id":"","label":"Time","pattern":"","type":"string"},
    {"id":"","label":"SP02","pattern":"","type":"number"},
    {"id":"","label":"Pulse","pattern":"","type":"number"}
  ],
  "rows": [
)=====";

const char graph_json_footer[] PROGMEM = R"=====(
  ]
}
)=====";

