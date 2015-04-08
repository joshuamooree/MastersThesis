function ocv=OCVfromSOCtemp(soc,temp,model)
% This function returns the fully rested open-circuit-voltage of an LiPB
% cell given its soc.

sochoriz = soc(:)'; % force soc to be row-vector
if isscalar(temp), 
  temphoriz = temp*ones(size(sochoriz)); 
else
  temphoriz = temp(:)';
end
diffSOC=model.SOC(2)-model.SOC(1);
ocv=zeros(size(sochoriz));
I1=find(sochoriz <= model.SOC(1));
I2=find(sochoriz >= model.SOC(end));
I3=find(sochoriz > model.SOC(1) & sochoriz < model.SOC(end));

% for voltages less than 0% soc... 07/26/06
% extrapolate off low end of table
dv = (model.OCV0(2)+temphoriz.*model.OCVrel(2)) - (model.OCV0(1)+temphoriz.*model.OCVrel(1));
ocv(I1)= sochoriz(I1).*dv(I1)/diffSOC + model.OCV0(1)+temphoriz(I1).*model.OCVrel(1);

% for voltages greater than 100% soc... 07/26/06
% extrapolate off high end of table
dv = (model.OCV0(end)+temphoriz.*model.OCVrel(end)) - (model.OCV0(end-1)+temphoriz.*model.OCVrel(end-1));
ocv(I2) = (sochoriz(I2)-1).*dv(I2)/diffSOC + model.OCV0(end)+temphoriz(I2).*model.OCVrel(end);

% for normal soc range...
% manually interpolate (10x faster than "interp1")
I4=sochoriz(I3)/diffSOC;
I5=floor(I4);
ocv(I3)=model.OCV0(I5+1).*(1-(I4-I5)) + model.OCV0(I5+2).*(I4-I5);
ocv(I3)=ocv(I3) + temphoriz(I3).*(model.OCVrel(I5+1).*(1-(I4-I5)) + model.OCVrel(I5+2).*(I4-I5));
ocv = reshape(ocv,size(soc));