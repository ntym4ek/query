(function ($) {
  Drupal.behaviors.prod = {
    attach: function (context, settings) {

      // добавить тень фиксированным колонкам -------------------------------------------------------------
      $('.year-plan-fact-page .table-wrapper').on('scroll', (event) => {
        if (event.target.scrollLeft > 0) {
          $('.fixed-last').addClass('fixed-shadow');
        } else {
          $('.fixed-last').removeClass('fixed-shadow');
        }
      });

      // скроллинг таблиц мышью -----------------------------------------------------------------------------
      const table = document.querySelector('.table-wrapper');
      table.style.cursor = 'grab';
      let pos = { top: 0, left: 0, x: 0, y: 0 };

      const mouseDownHandler = function (e) {
        pos = {
          // The current scroll
          left: table.scrollLeft,
          top: table.scrollTop,
          // Get the current mouse position
          x: e.clientX,
          y: e.clientY,
        };
        document.addEventListener('mousemove', mouseMoveHandler);
        document.addEventListener('mouseup', mouseUpHandler);
        table.style.cursor = 'grabbing';
        table.style.userSelect = 'none';
      };
      const mouseMoveHandler = function (e) {
        const dx = e.clientX - pos.x;
        const dy = e.clientY - pos.y;
        table.scrollTop = pos.top - dy;
        table.scrollLeft = pos.left - dx;
      };
      const mouseUpHandler = function () {
        document.removeEventListener('mousemove', mouseMoveHandler);
        document.removeEventListener('mouseup', mouseUpHandler);
        table.style.cursor = 'grab';
        table.style.removeProperty('user-select');
      };

      // Attach the handler
      table.addEventListener('mousedown', mouseDownHandler);
    }
  };
})(jQuery);
