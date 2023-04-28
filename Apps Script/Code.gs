const stations = ["KBEH","KLWA","KAZO","KOEB","KJXN","KARB","KDUH","KDTW","KMTC","KPTK","KOZW","KLAN","KGRR","KBIV","KMKG","KFFX","KRQB","KAMN","KFNT","KD95","KPHN","KP58","KBAX","KCFS","KHYX","KIKW","KOSC","KAPN","KGLR","KHTL","KCAD","KMBL","KLDM","KTVC","KCVX","KSLH","KCIU","KANJ","KERY","KISQ","KP53","KSAW","KESC","KMNM","KIMT","KIWD","KCMX","KP59"];
const fallbackOnForecastData = false;
const disableCache = false;
const cache = CacheService.getScriptCache();

function doGet(request) {
  let cached = cache.getAll(stations); // get all cache entries
  let colors = stations.map(station => {
    try {
      return JSON.parse(cached[station]); // attempt to get corresponding cache entry
    }
    catch(e) {
      return [0,0,0]; // fall back on black
    }
  });
  if (request?.parameter?.hex) colors = colors.map(color => chroma(color).hex());
  if (request?.parameter?.callback) {
    return ContentService.createTextOutput(request?.parameter?.callback + '(' + JSON.stringify(colors) + ')').setMimeType(ContentService.MimeType.JAVASCRIPT);
  } else {
    return ContentService.createTextOutput(JSON.stringify(colors)).setMimeType(ContentService.MimeType.JSON);
  }
}

function printCache() {
  let cached = cache.getAll(stations);
  console.log(JSON.stringify(cached));
  let colors = stations.map(station => {
    try {
      return JSON.parse(cached[station]);
    }
    catch(e) {
      return [0,0,0];
    }
  }); // find corresponding cache entry
  console.log(JSON.stringify(colors));
  colors = colors.map(color => chroma(color).hex());
  console.log(JSON.stringify(colors));
}

function refresh() {

  let now = new Date();
  if (now.getHours() <= 6 || now.getHours() >= 22) {
    cache.removeAll(stations);
    return;
  }

  // const tempScale = chroma.scale(["#111111","#21006B","#4C006B","#6B006B","#990099","#B300B3","#CC00CC","#E600E6","#FF02FF","#D100FF","#9E01FF","#6600FF","#1800FF","#144AFF","#0E74FF","#00A4FF","#00CBFF","#00E6FF","#00FFFF","#01FFB3","#7FFF00","#CEFF00","#FEFF00","#FFE601","#FFCB00","#FFAE00","#FF9900","#FE7F00","#FF4F00","#FF0700","#FF4545","#FF6968","#FF8787","#FF9E9E","#FFB5B5","#FFCFCF","#FFE8E8","#EEEEEE"]).domain([-60,125]).mode('hsl'); // wunderground scale
  // const tempScale = chroma.scale(["#ffffff","#ffd2ff","#df8ee2","#a760c8","#5e4eac","#37c4e2","#33de9d","#7ed634","#ffff32","#fb9232","#da4f33","#b23833","#000000"]).domain([-10,110]).mode('hsl'); // nws scale
  const tempScale = chroma.scale(["#734669","#caacc3","#a24691","#8f59a9","#9ddbd9","#6abfb5","#64a6bd","#5d85c6","#447d63","#809318","#f3b704","#e85319","#470e00"]).domain([-94,-67,-40,-13,5,17,25,32,34,50,70,86,116]).mode('hsl'); // windy.com default

  if (disableCache) cache.removeAll(stations);

  for (let station of stations) {
    let temp = getTemp_(station);
    if (temp) cache.put(station, JSON.stringify(tempScale(temp).rgb()), 3600); // 1 hour
    Utilities.sleep(200);
  };

  return;

}

function getTemp_(id) {

  console.log('Fetching ' + id + '...');

  let conditions;
  try {
    conditions = getJSON_('https://api.weather.gov/stations/' + id + '/observations/latest', {"Accept": "application/json", "User-Agent": "(https://herzog.tech, xd1936@gmail.com)"});
  }
  catch(e) {
    return null; // error accessing API
  }

  if (fallbackOnForecastData && !conditions?.properties?.temperature?.value && !cache.get(id)) {
    console.log('No temp on this check and nothing cached. Falling back on forecast data...');
    try {
      let point = getJSON_('https://api.weather.gov/points/' + conditions.geometry.coordinates[1] + ',' + conditions.geometry.coordinates[0], {"Accept": "application/json", "User-Agent": "(https://herzog.tech, xd1936@gmail.com)"});
      let forecast = getJSON_(point.properties.forecastHourly, {"Accept": "application/json", "User-Agent": "(https://herzog.tech, xd1936@gmail.com)"});
      return forecast.properties.periods[0].temperatureUnit === 'F' ? forecast.properties.periods[0].temperature : new Number(forecast.properties.periods[0].temperature).cToF();
    }
    catch(e) {
      return null;
    }
  }

  if (conditions?.properties?.temperature?.value == null) return null; // specifically ==, so we check for null and undefined, but not a temp of zero

  let oneHourFromNow = new Date();
  oneHourFromNow.setHours(oneHourFromNow.getHours() + 1);
  if (new Date(conditions?.properties?.timestamp) > oneHourFromNow) return null;

  return conditions?.properties?.temperature?.unitCode === 'wmoUnit:degC' ? new Number(conditions?.properties?.temperature?.value).cToF() : conditions?.properties?.temperature?.value;

}

function getJSON_(url, headers) {
  
  if (!headers) {
    headers = {};
  }
  
  let json;
  try {
    json = UrlFetchApp.fetch(url, {"headers": headers}).getContentText();
    json = JSON.parse(json);
  }
  catch(err) { // sometimes the api times out. try again if so
    try {
      Utilities.sleep(5000);
      json = UrlFetchApp.fetch(url, {"headers": headers}).getContentText();
      json = JSON.parse(json);
    }
    catch(err) {
      throw 'Problem fetching ' + url;
    }
  }
  
  return json;
  
}

Number.prototype.cToF = function() { return (9 / 5) * this + 32; }
