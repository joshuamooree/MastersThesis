function theParam = getParam(paramName,temperature,model)
% function theParam = getParam(paramName,temperature,model)
%
% This function returns the values of the specified ESC cell-model
% parameter 'paramName' for the temperatures in 'temperature' for the cell
% model data stored in 'model'.  'paramName' may be one of 'BParam',
% 'CParam', 'CnParam', 'RParam', or 'MParam' (not case sensitive).

switch upper(paramName)
  case 'BPARAM',
    theParam = ...
      model.BParam(1)*temperature.^2+model.BParam(2)*temperature+...
      model.BParam(3)+model.BParam(4).*exp(temperature.*model.BParam(5));
  case 'CPARAM',
    theParam = ...
      model.CParam(1)*temperature.^2+model.CParam(2)*temperature+...
      model.CParam(3)+model.CParam(4).*exp(temperature*model.CParam(5));
  case 'CNNONLIN',
    theParam = ...
      model.CnNonlin(1)*temperature.^2+model.CnNonlin(2)*temperature+...
      model.CnNonlin(3)+model.CnNonlin(4).*exp(temperature*model.CnNonlin(5));
  case 'CNPARAM',
    theParam = interp1(model.temps,model.CnParam,temperature,'spline');
  case 'RPARAM',
    theParam = interp1(model.temps,model.RParam,temperature,'spline');
  case 'MPARAM',
    theParam = interp1(model.temps,model.MParam,temperature,'spline');
  otherwise
    error('Bad argument to "paramName"');
end
