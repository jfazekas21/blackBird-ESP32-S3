// Asynchronous function to fetch data from a JSON file
async function fetchData() {
  try {
    const response = await fetch('config.json');
    const data = await response.json();
    return data;
  } catch (error) {
    console.error('Error fetching data:', error);
    return null;
  }
}
function updateVersionNumber(version) {
    // Find the element with the class 'js-esp_communications'
    const versionElement = document.querySelector('.js-esp_communications');
    
    // Check if the element exists
    if (versionElement) {
        // Update the text content of the element with the new version number
        versionElement.textContent = `Currently running version ${version}`;
    } else {
        console.error('Element with class "js-esp_communications" not found.');
    }
}

// Example usage:
const newVersionNumber = '4.22'; // Replace '4.22' with the actual new version number
updateVersionNumber(newVersionNumber);

// Function to update HTML content with fetched data
async function updateHTML() {
  // Fetch data using the fetchData function
  const data = await fetchData();

  // Check if data is successfully fetched
  if (data) {
    // Update page title with data from config.json
    document.title = data.titles.pageTitle;

    // Log the fetched data to the console for debugging
    console.log(data);

    // Extract font color from data
    const fontColor = data.colors.fontColor.fullHtml;

    const elements = document.querySelectorAll('*');

    elements.forEach((element) => {
      try {
        element.style.color = fontColor;
      } catch (error) {
        console.error(
          `Error setting color for element: ${element.tagName}`,
          error
        );
      }
    });

    const bannerContainerElement = document.querySelector('.banner-container');
    bannerContainerElement.style.backgroundColor =
      data.colors.bannerBackgroundColor;

    const menuItemElement = document.querySelector('.menu-item');
    menuItemElement.style.color = data.colors.fontColor.productText;
    const activeItemElement = document.querySelector('.active');
    activeItemElement.style.color = data.colors.fontColor.fullHtml;

    const titleInfoElements = document.querySelectorAll('.title-info');
    titleInfoElements.forEach((titleInfoElement) => {
      titleInfoElement.style.color = data.colors.fontColor.productTitle;
    });

    const textInfoElements = document.querySelectorAll('.text-info');
    textInfoElements.forEach((textInfoElement) => {
      textInfoElement.style.backgroundColor = data.colors.productTextBackground;
    });

    const spanInfoElements = document.querySelectorAll('.span-info');
    spanInfoElements.forEach((textInfoElement) => {
      textInfoElement.style.color = data.colors.fontColor.productText;
    });

    // Update specific elements based on their class selectors
    const bannerImageElement = document.querySelector(
      '.js-img-header-container'
    );
    if (bannerImageElement) {
      const imgElement = document.createElement('img');
      imgElement.src = data.optional.bannerImageFilePath;
      imgElement.width = data.optional.bannerImageSize.width;
      imgElement.alt = data.titles.pageTitle;
      imgElement.classList.add('d-block');
      bannerImageElement.appendChild(imgElement);
    } else {
      console.error('Element with class "js-img-header-container" not found.');
    }
    const manufacturerNameElement = document.querySelector(
      '.js-manufacturerName'
    );
    if (manufacturerNameElement) {
      manufacturerNameElement.textContent = data.titles.manufacturerName;
    } else {
      console.error('Element with class "js-manufacturerName" not found.');
    }

    const productTitleElement = document.querySelector('.js-productTitle');
    if (productTitleElement) {
      productTitleElement.textContent = data.titles.pageTitle;
      productTitleElement.style.color = data.colors.fontColor.pageTitle;
    } else {
      console.error('Element with class "js-productTitle" not found.');
    }

    const hardwareRevisionElement = document.querySelector(
      '.js-hardwareRevision'
    );
    if (hardwareRevisionElement) {
      hardwareRevisionElement.textContent = data.titles.HardwareRevision;
    } else {
      console.error('Element with class "js-hardwareRevision" not found.');
    }

    const productModelNameElement = document.querySelector(
      '.js-productModelName'
    );
    if (productModelNameElement) {
      productModelNameElement.textContent = data.titles.productmodelname;
    } else {
      console.error('Element with class "productModelName" not found.');
    }

    const firmwareElement = document.querySelector('.js-firmwareVersion');
    if (firmwareElement) {
      firmwareElement.textContent = data.titles.FirmwareVersion;
    } else {
      console.error('Element with class "firmwareVersion" not found.');
    }

    const deviceIDElement = document.querySelector('.js-deviceId');
    if (deviceIDElement) {
      deviceIDElement.textContent = data.titles.DeviceID;
    } else {
      console.error('Element with class "deviceId" not found.');
    }

    const COPUpdateContainerElement = document.querySelector(
      '.COPUpdateContainer'
    );
    if (COPUpdateContainerElement) {
      COPUpdateContainerElement.style.display = data.enableCOPUpdateButton
        ? 'inherit'
        : 'none';
    } else {
      console.error('Element with class "COPUpdateContainer" not found.');
    }

    // Set the background color of the body
    document.body.style.backgroundColor = data.colors.backgroundColor;

    // Update font color of the entire document
    document.body.style.color = fontColor;
  }

  // Check onChange value for each input file
  document.getElementById('file-upload_esp_communications').onchange =
    function () {
      var name = document.getElementById('file-upload_esp_communications');
      document.querySelector('.js-espFirmwareFile').textContent =
        name.files.item(0).name;
    };

  document.getElementById('file-upload_coprocessor').onchange = function () {
    var name = document.getElementById('file-upload_coprocessor');
    document.querySelector('.js-coprocessorFirmwareFile').textContent =
      name.files.item(0).name;
  };
}

// Install Update buttons functionality
function espFirmwareFileInstallUpdate(e) {
  //e.preventDefault();
  //alert('ESP Firmware File Install Update');
  // Enter functionality of install update here
  
  
    var fileInput = document.getElementById('file-upload_esp_communications');
    
    // Create a new FormData object
    var formData = new FormData();
    
    // Append the file to the FormData object
   // formData.append('firmwareFile', fileInput.files[0]); // Assuming only one file is selected
     var file = fileInput.files[0];
                formData.set("file", file, file.name);
   // var xhr = new XMLHttpRequest();
    
                    var xhr = new XMLHttpRequest();

                //Upload progress
      xhr.upload.addEventListener("progress", function(evt) {
        if (evt.lengthComputable) {
            var percentComplete = (evt.loaded / evt.total) * 100;
            var x = Math.floor(percentComplete);
            
            // Update progress text
            document.querySelector('.progress-text').textContent = x + "%";

            // Update progress bar width
            document.querySelector('.progress-bar').style.width = x + "%";

            // After completion, handle further actions
            if (x == 100) {
            document.querySelector('.progress-text').textContent = "System is Rebooting please wait ...";
               // getstatus();
            }
        } else {
            window.alert('total size is unknown');
        }
    }, false);
    
    
    // Prepare the request
    xhr.open('POST', '/update');

    // Set up an event listener to handle the response
    xhr.onload = function() {
        if (xhr.status === 200) {
            // Request was successful, handle the response here
            console.log('Firmware update successful');
        } else {
            // Request failed, handle error here
            console.error('Firmware update failed');
        }
    };

    // Set up an event listener to handle errors
    xhr.onerror = function() {
        // Handle error here
        console.error('Error encountered while updating firmware');
    };

    // Send the request
      xhr.send(formData);
}

function coprocessorFirmwareFileInstallUpdate(e) {
  e.preventDefault();
  alert('Co-processor Firmware File Install Update');
  // Enter functionality of install update here
}

// Call the updateHTML function when the DOM content is fully loaded
document.addEventListener('DOMContentLoaded', updateHTML);
